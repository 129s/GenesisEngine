#include "genesis/agents/ActionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "genesis/agents/CarriedResources.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"
#include "genesis/world/MapPathfinding.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::agents {

namespace {
constexpr float kMinSpeed = 0.1f;
constexpr float kCrossMapPenalty = 500.0f;
constexpr std::uint32_t kMaxPlanningRetries = 2;
constexpr int kMaxAcquireDepth = 4;

using json = nlohmann::json;

struct WorkshopInput {
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    std::uint32_t units{1};
};

struct WorkshopRecipe {
    std::uint32_t outputUnits{1};
    std::vector<WorkshopInput> inputs{};
};

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

    if (workshop.contains("outputUnits") && workshop.at("outputUnits").is_number_unsigned()) {
        recipe.outputUnits = std::max<std::uint32_t>(1U, workshop.at("outputUnits").get<std::uint32_t>());
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
        if (!item.contains("units") || !item.at("units").is_number_unsigned()) {
            continue;
        }
        const auto type = genesis::world::parseResourceType(item.at("type").get<std::string>());
        if (!type) {
            continue;
        }
        const auto units = item.at("units").get<std::uint32_t>();
        if (units == 0U) {
            continue;
        }
        recipe.inputs.push_back(WorkshopInput{*type, units});
    }

    if (recipe.inputs.empty()) {
        return std::nullopt;
    }

    return recipe;
}

std::uint32_t ceilDiv(std::uint32_t a, std::uint32_t b) {
    if (b == 0U) return 0U;
    return (a + (b - 1U)) / b;
}
} // namespace

ActionExecutor::ActionExecutor(genesis::world::WorldDatabase& db, genesis::world::system::ResourceSystem& resources)
    : m_db(db)
    , m_resources(resources) {}

void ActionExecutor::requestMoveToInteraction(entt::entity entity,
                                              genesis::world::InteractionId target,
                                              float speed,
                                              entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (!queue.tasks.empty()) {
        return;
    }

    ActionTask move{};
    move.type = ActionType::MoveToInteraction;
    move.interaction = target;
    move.speed = std::max(speed, kMinSpeed);
    queue.tasks.push_back(move);
}

void ActionExecutor::requestConsume(entt::entity entity,
                                    genesis::world::InteractionId interaction,
                                    NeedType need,
                                    genesis::world::ResourceType type,
                                    std::uint32_t amount,
                                    float reliefPerUnit,
                                    entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (hasPendingConsume(queue, interaction, type)) {
        return;
    }

    if (queue.tasks.empty()) {
        const auto* agentLocation = registry.try_get<components::AgentLocation2D>(entity);
        bool atTarget = false;
        genesis::world::MapId targetMap{0};
        float tx = 0.0f, ty = 0.0f;
        if (auto it = m_db.findInteraction(interaction)) {
            targetMap = it->mapId;
            const auto coord = it->worldCoord();
            tx = static_cast<float>(coord.first);
            ty = static_cast<float>(coord.second);
            if (agentLocation) {
                atTarget = (agentLocation->mapId == targetMap && agentLocation->x == tx && agentLocation->y == ty);
            }
        }
        if (!atTarget) {
            ActionTask move{};
            move.type = ActionType::MoveToInteraction;
            move.interaction = interaction;
            move.speed = 1.0f;
            queue.tasks.push_back(move);
        }
    }

    ActionTask consume{};
    consume.type = ActionType::ConsumeResource;
    consume.interaction = interaction;
    consume.need = need;
    consume.resource = type;
    consume.amount = amount;
    consume.retries = 0;
    consume.reliefPerUnit = reliefPerUnit;
    queue.tasks.push_back(consume);
}

void ActionExecutor::update(entt::registry& registry, float /*deltaSeconds*/) {
    auto view = registry.view<ActionQueue, components::AgentLocation2D>();

    for (auto entity : view) {
        auto& queue = view.get<ActionQueue>(entity);
        auto& location = view.get<components::AgentLocation2D>(entity);

        bool advanced = true;
        while (advanced && !queue.tasks.empty()) {
            auto& task = queue.tasks.front();
            advanced = false;

            switch (task.type) {
            case ActionType::MoveToInteraction:
                processMove(entity, queue, location, registry);
                advanced = false;
                break;
            case ActionType::ConsumeResource:
                processConsume(entity, queue, location, registry);
                advanced = true;
                break;
            case ActionType::TakeResource:
                processTake(entity, queue, location, registry);
                advanced = true;
                break;
            case ActionType::ProduceResource:
                processProduce(entity, queue, location, registry);
                advanced = true;
                break;
            }
        }

        if (queue.tasks.empty()) {
            registry.remove<ActionQueue>(entity);
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
        }
    }
}

bool ActionExecutor::hasPendingActions(entt::entity entity, const entt::registry& registry) const {
    if (!registry.all_of<ActionQueue>(entity)) {
        return false;
    }
    const auto& queue = registry.get<ActionQueue>(entity);
    return !queue.tasks.empty();
}

void ActionExecutor::ensureQueue(entt::entity entity, entt::registry& registry) {
    if (!registry.all_of<ActionQueue>(entity)) {
        registry.emplace<ActionQueue>(entity);
    }
}

bool ActionExecutor::hasPendingConsume(const ActionQueue& queue,
                                       genesis::world::InteractionId interaction,
                                       genesis::world::ResourceType type) const {
    return std::any_of(queue.tasks.begin(), queue.tasks.end(), [&](const ActionTask& task) {
        return task.type == ActionType::ConsumeResource && task.interaction == interaction && task.resource == type;
    });
}

void ActionExecutor::processMove(entt::entity entity,
                                 ActionQueue& queue,
                                 components::AgentLocation2D& location,
                                 entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (location.mapId == targetMap && location.x == tx && location.y == ty) {
        queue.tasks.pop_front();
        if (registry.any_of<components::MovementIntent2D>(entity)) {
            registry.remove<components::MovementIntent2D>(entity);
        }
        return;
    }

    if (location.mapId != targetMap) {
        const auto path = genesis::world::shortestMapPath(m_db, location.mapId, targetMap);
        if (!path || path->maps.size() < 2) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        const auto nextMap = path->maps[1];
        std::optional<genesis::world::Portal> bestPortal;
        float bestCost = std::numeric_limits<float>::infinity();

        for (const auto& portal : m_db.portals(location.mapId)) {
            if (portal.targetMapId != nextMap) {
                continue;
            }
            auto portalInter = m_db.findInteraction(portal.interactionId);
            if (!portalInter) {
                continue;
            }
            const auto portalCoord = portalInter->worldCoord();
            const float px = static_cast<float>(portalCoord.first);
            const float py = static_cast<float>(portalCoord.second);
            const float dx = px - location.x;
            const float dy = py - location.y;
            const float dist2 = dx * dx + dy * dy;
            const float cost = dist2 + static_cast<float>(std::max(0.0, portal.teleportCost));
            if (cost < bestCost) {
                bestCost = cost;
                bestPortal = portal;
            }
        }

        if (!bestPortal) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        auto portalInter = m_db.findInteraction(bestPortal->interactionId);
        if (!portalInter) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        const auto portalCoord = portalInter->worldCoord();
        const float px = static_cast<float>(portalCoord.first);
        const float py = static_cast<float>(portalCoord.second);

        auto& intent = registry.get_or_emplace<components::MovementIntent2D>(entity);
        if (location.x == px && location.y == py) {
            const auto targetCoord = bestPortal->targetCoord.value_or(std::make_pair(0, 0));
            intent.targetMapId = bestPortal->targetMapId;
            intent.targetX = static_cast<float>(targetCoord.first);
            intent.targetY = static_cast<float>(targetCoord.second);
            intent.speed = 0.0f;
        } else {
            intent.targetMapId = location.mapId;
            intent.targetX = px;
            intent.targetY = py;
            intent.speed = std::max(task.speed, kMinSpeed);
        }
        return;
    }

    auto& intent = registry.get_or_emplace<components::MovementIntent2D>(entity);
    intent.targetMapId = targetMap;
    intent.targetX = tx;
    intent.targetY = ty;
    intent.speed = std::max(task.speed, kMinSpeed);
}

void ActionExecutor::processConsume(entt::entity entity,
                                    ActionQueue& queue,
                                    components::AgentLocation2D& location,
                                    entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        ActionTask move{};
        move.type = ActionType::MoveToInteraction;
        move.interaction = task.interaction;
        move.speed = 1.0f;
        queue.tasks.emplace(queue.tasks.begin(), move);
        return;
    }

    auto* needs = registry.try_get<NeedComponent>(entity);
    if (!needs) {
        queue.tasks.pop_front();
        return;
    }

    auto* state = needs->needs.state(task.need);
    const auto* descriptor = needs->needs.descriptor(task.need);
    if (!state || !descriptor) {
        queue.tasks.pop_front();
        return;
    }

    const auto consumed = m_resources.consumeFromInteraction(registry, task.interaction, task.resource, task.amount);
    if (consumed > 0U) {
        const float relief = static_cast<float>(consumed) * task.reliefPerUnit;
        state->value = std::max(descriptor->minValue, state->value - relief);
        state->clamp(*descriptor);
        needs->lastSamples[needIndex(task.need)] = std::nullopt;
    }

    if (consumed == 0U) {
        const auto inter = m_db.findInteraction(task.interaction);
        const auto recipe = inter ? parseWorkshopRecipe(*inter) : std::nullopt;
        if (recipe && task.retries < kMaxPlanningRetries) {
            auto* carried = registry.try_get<components::CarriedResources>(entity);
            if (!carried) {
                carried = &registry.emplace<components::CarriedResources>(entity);
            }

            const std::uint32_t batches = ceilDiv(task.amount, recipe->outputUnits);

            std::vector<ActionTask> plan;
            plan.reserve(8);

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
                            cost = kCrossMapPenalty + static_cast<float>(path->totalCost);
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
                if (depth > kMaxAcquireDepth) {
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
                        acquire(input.type, input.units * needBatches, depth + 1);
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
                const std::uint32_t have = carried->get(input.type);
                if (required > have) {
                    acquire(input.type, required - have, 0);
                }
            }

            ActionTask prod{};
            prod.type = ActionType::ProduceResource;
            prod.interaction = task.interaction;
            prod.resource = task.resource;
            prod.batches = batches;
            plan.push_back(prod);

            ActionTask retry = task;
            retry.retries = task.retries + 1;
            plan.push_back(retry);

            queue.tasks.pop_front();
            queue.tasks.insert(queue.tasks.begin(), plan.begin(), plan.end());
            return;
        }
    }

    queue.tasks.pop_front();
}

void ActionExecutor::processTake(entt::entity entity,
                                 ActionQueue& queue,
                                 components::AgentLocation2D& location,
                                 entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        ActionTask move{};
        move.type = ActionType::MoveToInteraction;
        move.interaction = task.interaction;
        move.speed = 1.0f;
        queue.tasks.emplace(queue.tasks.begin(), move);
        return;
    }

    auto* carried = registry.try_get<components::CarriedResources>(entity);
    if (!carried) {
        carried = &registry.emplace<components::CarriedResources>(entity);
    }

    const auto want = std::min(task.amount, carried->remaining(task.resource));
    if (want == 0U) {
        queue.tasks.pop_front();
        return;
    }

    const auto taken = m_resources.consumeFromInteraction(registry, task.interaction, task.resource, want);
    carried->add(task.resource, taken);
    queue.tasks.pop_front();
}

void ActionExecutor::processProduce(entt::entity entity,
                                    ActionQueue& queue,
                                    components::AgentLocation2D& location,
                                    entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        ActionTask move{};
        move.type = ActionType::MoveToInteraction;
        move.interaction = task.interaction;
        move.speed = 1.0f;
        queue.tasks.emplace(queue.tasks.begin(), move);
        return;
    }

    const auto recipe = parseWorkshopRecipe(*it);
    if (!recipe) {
        queue.tasks.pop_front();
        return;
    }

    auto* carried = registry.try_get<components::CarriedResources>(entity);
    if (!carried) {
        queue.tasks.pop_front();
        return;
    }

    const auto state = m_resources.spawnState(registry, task.interaction);
    if (!state) {
        queue.tasks.pop_front();
        return;
    }

    const auto outputUnits = std::max<std::uint32_t>(1U, recipe->outputUnits);
    const auto wantedBatches = std::max<std::uint32_t>(1U, task.batches);

    std::uint32_t maxByInputs = std::numeric_limits<std::uint32_t>::max();
    for (const auto& input : recipe->inputs) {
        if (input.units == 0U) {
            continue;
        }
        maxByInputs = std::min(maxByInputs, carried->get(input.type) / input.units);
    }

    const std::uint32_t space = (state->current >= state->capacity) ? 0U : (state->capacity - state->current);
    const std::uint32_t maxByCapacity = space / outputUnits;

    const std::uint32_t batches = std::min(wantedBatches, std::min(maxByInputs, maxByCapacity));
    if (batches == 0U) {
        queue.tasks.pop_front();
        return;
    }

    for (const auto& input : recipe->inputs) {
        carried->remove(input.type, input.units * batches);
    }

    m_resources.produceAtInteraction(registry, task.interaction, outputUnits * batches);
    queue.tasks.pop_front();
}

} // namespace genesis::agents
