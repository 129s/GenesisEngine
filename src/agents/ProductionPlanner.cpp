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

} // namespace

std::optional<WorkshopRecipe> parseWorkshopRecipe(const genesis::world::Interaction& interaction) {
    if (!interaction.meta) {
        return std::nullopt;
    }
    const auto& meta = *interaction.meta;
    if (!meta.contains("workshop")) {
        return std::nullopt;
    }
    const auto& workshop = meta.at("workshop");
    if (!workshop.is_object()) {
        return std::nullopt;
    }

    WorkshopRecipe recipe{};

    if (workshop.contains("outputUnits")) {
        const auto& value = workshop.at("outputUnits");
        if (value.is_number_unsigned()) {
            recipe.outputUnits = std::max<std::uint32_t>(1U, value.get<std::uint32_t>());
        } else if (value.is_number_integer()) {
            const auto v = value.get<std::int64_t>();
            if (v > 0) {
                recipe.outputUnits = static_cast<std::uint32_t>(v);
            }
        }
        recipe.outputUnits = std::max<std::uint32_t>(1U, recipe.outputUnits);
    }

    if (!workshop.contains("inputs") || !workshop.at("inputs").is_array()) {
        return std::nullopt;
    }

    for (const auto& item : workshop.at("inputs")) {
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
        recipe.inputs.push_back(WorkshopRecipeInput{*type, units});
    }

    if (recipe.inputs.empty()) {
        return std::nullopt;
    }

    return recipe;
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
    const auto recipe = parseWorkshopRecipe(*workshopInter);
    if (!recipe) {
        return std::nullopt;
    }

    const std::uint32_t batches = ceilDiv(request.outputAmount, recipe->outputUnits);
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

    std::array<bool, components::CarriedResources::kTypeCount> acquiring{};

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
        const auto targetRecipe = targetInter ? parseWorkshopRecipe(*targetInter) : std::nullopt;

        if (targetRecipe) {
            const std::uint32_t needBatches = ceilDiv(units, targetRecipe->outputUnits);
            for (const auto& input : targetRecipe->inputs) {
                if (!acquire(input.type, input.units * needBatches, depth + 1)) {
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

        if (idx < acquiring.size()) {
            acquiring[idx] = false;
        }
        return true;
    };

    for (const auto& input : recipe->inputs) {
        const std::uint32_t required = input.units * batches;
        const std::uint32_t have = carried.get(input.type);
        if (required > have) {
            acquire(input.type, required - have, 0);
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
