#include "genesis/agents/ProductionPlanner.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "genesis/world/MapPathfinding.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

namespace genesis::agents {

namespace {

using json = nlohmann::json;

std::uint32_t ceilDiv(std::uint32_t a, std::uint32_t b) {
    if (b == 0U) return 0U;
    return (a + (b - 1U)) / b;
}

std::uint32_t parsePositiveU32(const json& value, std::uint32_t fallback) {
    std::uint32_t out = fallback;
    if (value.is_number_unsigned()) {
        out = value.get<std::uint32_t>();
    } else if (value.is_number_integer()) {
        const auto v = value.get<std::int64_t>();
        if (v > 0) {
            out = static_cast<std::uint32_t>(v);
        }
    }
    return std::max<std::uint32_t>(1U, out);
}

std::optional<WorkshopRecipe> parseRecipeObject(const json& obj) {
    if (!obj.is_object()) {
        return std::nullopt;
    }

    WorkshopRecipe recipe{};
    if (obj.contains("outputUnits")) {
        recipe.outputUnits = parsePositiveU32(obj.at("outputUnits"), recipe.outputUnits);
    }

    if (!obj.contains("inputs") || !obj.at("inputs").is_array()) {
        return std::nullopt;
    }

    for (const auto& item : obj.at("inputs")) {
        if (!item.is_object()) {
            continue;
        }
        if (!item.contains("type") || !item.at("type").is_string()) {
            continue;
        }
        if (!item.contains("units") || !item.at("units").is_number()) {
            continue;
        }
        const auto type = genesis::world::parseResourceType(item.at("type").get<std::string>());
        if (!type) {
            continue;
        }
        std::uint32_t units = 0U;
        const auto& unitsValue = item.at("units");
        if (unitsValue.is_number_unsigned()) {
            units = unitsValue.get<std::uint32_t>();
        } else if (unitsValue.is_number_integer()) {
            const auto v = unitsValue.get<std::int64_t>();
            if (v > 0) {
                units = static_cast<std::uint32_t>(v);
            }
        }
        if (units == 0U) {
            continue;
        }
        bool consumable = true;
        if (item.contains("consumable") && item.at("consumable").is_boolean()) {
            consumable = item.at("consumable").get<bool>();
        }
        recipe.inputs.push_back(WorkshopRecipeInput{*type, units, consumable});
    }

    if (recipe.inputs.empty()) {
        return std::nullopt;
    }
    return recipe;
}

} // namespace

std::vector<WorkshopRecipe> parseWorkshopRecipes(const genesis::world::Interaction& interaction) {
    std::vector<WorkshopRecipe> recipes;
    if (!interaction.meta) {
        return recipes;
    }
    const auto& meta = *interaction.meta;
    if (!meta.contains("workshop")) {
        return recipes;
    }
    const auto& workshop = meta.at("workshop");
    if (!workshop.is_object()) {
        return recipes;
    }

    if (workshop.contains("recipes") && workshop.at("recipes").is_array()) {
        for (const auto& r : workshop.at("recipes")) {
            if (auto parsed = parseRecipeObject(r)) {
                recipes.push_back(std::move(*parsed));
            }
        }
        return recipes;
    }

    // 兼容旧协议：workshop 直接包含 outputUnits/inputs。
    if (auto parsed = parseRecipeObject(workshop)) {
        recipes.push_back(std::move(*parsed));
    }
    return recipes;
}

std::optional<WorkshopRecipe> parseWorkshopRecipe(const genesis::world::Interaction& interaction) {
    auto recipes = parseWorkshopRecipes(interaction);
    if (recipes.empty()) {
        return std::nullopt;
    }
    return recipes.front();
}

ProductionPlanner::ProductionPlanner(genesis::world::WorldDatabase& db, ProductionPlannerConfig config)
    : m_db(db)
    , m_config(config) {
    if (!std::isfinite(m_config.crossMapPenalty) || m_config.crossMapPenalty < 0.0f) {
        m_config.crossMapPenalty = 0.0f;
    }
    if (m_config.maxPlanningRetries == 0U) {
        m_config.maxPlanningRetries = 1U;
    }
    if (m_config.maxAcquireDepth < 0) {
        m_config.maxAcquireDepth = 0;
    }
}

std::optional<std::vector<ActionTask>> ProductionPlanner::buildRecoveryPlan(const Request& request) const {
    if (!request.location || !request.carried) {
        return std::nullopt;
    }
    if (request.targetWorkshop == 0 || request.outputAmount == 0U) {
        return std::nullopt;
    }
    if (request.currentRetries >= m_config.maxPlanningRetries) {
        return std::nullopt;
    }

    const auto workshopInter = m_db.findInteraction(request.targetWorkshop);
    if (!workshopInter || workshopInter->kind != genesis::world::InteractionKind::Resource) {
        return std::nullopt;
    }
    const auto recipes = parseWorkshopRecipes(*workshopInter);
    if (recipes.empty()) {
        return std::nullopt;
    }

    std::vector<ActionTask> plan;
    plan.reserve(8);

    const auto& location = *request.location;
    const auto& carried = *request.carried;

    auto pickInteraction = [&](genesis::world::ResourceType type) -> std::optional<genesis::world::InteractionId> {
        float bestCost = std::numeric_limits<float>::infinity();
        genesis::world::InteractionId best{0};

        for (const auto& m : m_db.maps()) {
            for (const auto& candidate : m_db.interactions(m.id)) {
                if (candidate.kind != genesis::world::InteractionKind::Resource) {
                    continue;
                }
                if (!candidate.resourceType || *candidate.resourceType != type) {
                    continue;
                }

                float cost = std::numeric_limits<float>::infinity();
                if (candidate.mapId == location.mapId) {
                    const auto c = candidate.worldCoord();
                    const float dx = static_cast<float>(c.first) - location.x;
                    const float dy = static_cast<float>(c.second) - location.y;
                    cost = dx * dx + dy * dy;
                } else {
                    const auto path = genesis::world::shortestMapPath(m_db, location.mapId, candidate.mapId);
                    if (!path) {
                        continue;
                    }
                    cost = m_config.crossMapPenalty + static_cast<float>(path->totalCost);
                }

                if (cost < bestCost) {
                    bestCost = cost;
                    best = candidate.id;
                }
            }
        }

        if (best == 0) {
            return std::nullopt;
        }
        return best;
    };

    auto pickInteractionCost = [&](genesis::world::ResourceType type) -> std::optional<float> {
        float bestCost = std::numeric_limits<float>::infinity();
        bool found = false;

        for (const auto& m : m_db.maps()) {
            for (const auto& candidate : m_db.interactions(m.id)) {
                if (candidate.kind != genesis::world::InteractionKind::Resource) {
                    continue;
                }
                if (!candidate.resourceType || *candidate.resourceType != type) {
                    continue;
                }

                float cost = std::numeric_limits<float>::infinity();
                if (candidate.mapId == location.mapId) {
                    const auto c = candidate.worldCoord();
                    const float dx = static_cast<float>(c.first) - location.x;
                    const float dy = static_cast<float>(c.second) - location.y;
                    cost = dx * dx + dy * dy;
                } else {
                    const auto path = genesis::world::shortestMapPath(m_db, location.mapId, candidate.mapId);
                    if (!path) {
                        continue;
                    }
                    cost = m_config.crossMapPenalty + static_cast<float>(path->totalCost);
                }

                found = true;
                bestCost = std::min(bestCost, cost);
            }
        }

        if (!found || !std::isfinite(bestCost)) {
            return std::nullopt;
        }
        return bestCost;
    };

    auto pickBestRecipe = [&](const std::vector<WorkshopRecipe>& candidates,
                              genesis::world::ResourceType outputType,
                              std::uint32_t outputAmount) -> const WorkshopRecipe* {
        const WorkshopRecipe* best = nullptr;
        double bestScore = std::numeric_limits<double>::infinity();

        for (const auto& r : candidates) {
            const std::uint32_t outUnits = std::max<std::uint32_t>(1U, r.outputUnits);
            const std::uint32_t batches = ceilDiv(outputAmount, outUnits);
            double totalCost = 0.0;

            for (const auto& input : r.inputs) {
                const std::uint32_t required = input.consumable ? (input.units * batches) : input.units;
                const std::uint32_t have = carried.get(input.type);
                const std::uint32_t missing = (required > have) ? (required - have) : 0U;
                if (missing == 0U) {
                    continue;
                }
                const auto cost = pickInteractionCost(input.type);
                if (!cost) {
                    totalCost = std::numeric_limits<double>::infinity();
                    break;
                }
                totalCost += static_cast<double>(*cost) * static_cast<double>(missing);
            }

            if (!std::isfinite(totalCost)) {
                continue;
            }

            // 轻微偏好：更高产出单位更少批次（减少搬运/生产动作次数）。
            totalCost += static_cast<double>(batches) * 0.01;
            if (totalCost < bestScore) {
                bestScore = totalCost;
                best = &r;
            }
        }

        (void)outputType;
        return best;
    };

    const WorkshopRecipe* recipe = pickBestRecipe(recipes, request.outputType, request.outputAmount);
    if (!recipe) {
        return std::nullopt;
    }

    const std::uint32_t batches = ceilDiv(request.outputAmount, std::max<std::uint32_t>(1U, recipe->outputUnits));

    std::array<bool, components::CarriedResources::kTypeCount> acquiring{};
    std::array<std::uint32_t, components::CarriedResources::kTypeCount> reservedNonConsumable{};

    std::function<bool(genesis::world::ResourceType, std::uint32_t, int)> acquire;
    acquire = [&](genesis::world::ResourceType type, std::uint32_t units, int depth) -> bool {
        if (units == 0U) {
            return true;
        }
        if (depth > m_config.maxAcquireDepth) {
            return false;
        }

        const auto idx = components::resourceTypeIndex(type);
        if (idx < acquiring.size() && acquiring[idx]) {
            return false;
        }
        if (idx < acquiring.size()) {
            acquiring[idx] = true;
        }

        const auto targetId = pickInteraction(type);
        if (!targetId) {
            if (idx < acquiring.size()) {
                acquiring[idx] = false;
            }
            return false;
        }

        const auto targetInter = m_db.findInteraction(*targetId);
        const auto targetRecipes = targetInter ? parseWorkshopRecipes(*targetInter) : std::vector<WorkshopRecipe>{};
        const WorkshopRecipe* targetRecipe = targetRecipes.empty() ? nullptr : pickBestRecipe(targetRecipes, type, units);

        if (targetRecipe) {
            const std::uint32_t needBatches = ceilDiv(units, std::max<std::uint32_t>(1U, targetRecipe->outputUnits));
            for (const auto& input : targetRecipe->inputs) {
                const std::uint32_t required = input.consumable ? (input.units * needBatches) : input.units;
                const auto inIdx = components::resourceTypeIndex(input.type);
                const std::uint32_t effectiveHave = carried.get(input.type) + ((inIdx < reservedNonConsumable.size()) ? reservedNonConsumable[inIdx] : 0U);
                const std::uint32_t missing = (required > effectiveHave) ? (required - effectiveHave) : 0U;
                if (missing == 0U) {
                    continue;
                }
                if (!acquire(input.type, missing, depth + 1)) {
                    break;
                }
            }

            ActionTask prod{};
            prod.type = ActionType::ProduceResource;
            prod.interaction = *targetId;
            prod.resource = type;
            prod.batches = needBatches;
            plan.push_back(prod);
        }

        ActionTask take{};
        take.type = ActionType::TakeResource;
        take.interaction = *targetId;
        take.resource = type;
        take.amount = units;
        plan.push_back(take);

        if (idx < reservedNonConsumable.size()) {
            reservedNonConsumable[idx] += units;
        }

        if (idx < acquiring.size()) {
            acquiring[idx] = false;
        }
        return true;
    };

    for (const auto& input : recipe->inputs) {
        const std::uint32_t required = input.consumable ? (input.units * batches) : input.units;
        const auto idx = components::resourceTypeIndex(input.type);
        const std::uint32_t effectiveHave = carried.get(input.type) + ((idx < reservedNonConsumable.size()) ? reservedNonConsumable[idx] : 0U);
        if (required > effectiveHave) {
            acquire(input.type, required - effectiveHave, 0);
        }
    }

    ActionTask prod{};
    prod.type = ActionType::ProduceResource;
    prod.interaction = request.targetWorkshop;
    prod.resource = request.outputType;
    prod.batches = batches;
    plan.push_back(prod);

    ActionTask retry{};
    retry.type = ActionType::ConsumeResource;
    retry.interaction = request.targetWorkshop;
    retry.need = request.need;
    retry.resource = request.outputType;
    retry.amount = request.outputAmount;
    retry.retries = request.currentRetries + 1;
    retry.reliefPerUnit = request.reliefPerUnit;
    plan.push_back(retry);

    return plan;
}

} // namespace genesis::agents
