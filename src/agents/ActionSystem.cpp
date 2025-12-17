#include "genesis/agents/ActionSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "genesis/agents/CarriedResources.hpp"
#include "genesis/agents/Outcomes.hpp"
#include "genesis/agents/Experience.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/ProductionPlanner.hpp"
#include "genesis/world/MapPathfinding.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

namespace genesis::agents {

namespace {
constexpr float kMinSpeed = 0.1f;
constexpr const char* kReasonMissingInteraction = "MissingInteraction";
constexpr const char* kReasonNoRecipes = "NoRecipes";
constexpr const char* kReasonNoCarriedResources = "NoCarriedResources";
constexpr const char* kReasonNoSpawnState = "NoSpawnState";
constexpr const char* kReasonOutputFull = "OutputFull";
constexpr const char* kReasonMissingConsumableInput = "MissingConsumableInput";
constexpr const char* kReasonMissingNonConsumableInput = "MissingNonConsumableInput";
constexpr const char* kReasonUnknown = "Unknown";

constexpr const char* kReasonStockout = "Stockout";
constexpr const char* kReasonPlanningFailed = "PlanningFailed";
constexpr const char* kReasonUnreachable = "Unreachable";
constexpr const char* kReasonPreemptedCriticalNeed = "PreemptedCriticalNeed";

[[nodiscard]] std::string preemptedWithNeed(const char* needName) {
    std::string out(kReasonPreemptedCriticalNeed);
    out.push_back(':');
    out.append(needName);
    return out;
}

components::AgentOutcomeBuffer& ensureOutcomeBuffer(entt::entity entity, entt::registry& registry) {
    return registry.get_or_emplace<components::AgentOutcomeBuffer>(entity);
}

void recordCriticalPreempt(entt::entity entity, NeedType need, entt::registry& registry) {
    auto& outcomes = ensureOutcomeBuffer(entity, registry);
    outcomes.criticalPreempts.push_back(CriticalPreemptOutcome{need});
}

void recordResourceAttempt(entt::entity entity,
                           const ActionTask& task,
                           const telemetry::ResourceAttemptSnapshot& attempt,
                           ResourceAttemptFailure failure,
                           entt::registry& registry) {
    auto& outcomes = ensureOutcomeBuffer(entity, registry);
    ResourceAttemptOutcome o{};
    o.interaction = attempt.interactionId;
    o.resourceType = attempt.resourceType;
    o.need = task.need;
    o.wantedUnits = attempt.wantedUnits;
    o.obtainedUnits = attempt.obtainedUnits;
    o.recoveryPlanned = attempt.recoveryPlanned;
    o.failure = failure;
    outcomes.resourceAttempts.push_back(o);
}

[[nodiscard]] const char* needTypeName(NeedType need) {
    switch (need) {
    case NeedType::Hunger:
        return "Hunger";
    case NeedType::Thirst:
        return "Thirst";
    case NeedType::Energy:
        return "Energy";
    case NeedType::Social:
        return "Social";
    case NeedType::Count:
        break;
    }
    return "Unknown";
}

[[nodiscard]] bool isVitalNeed(NeedType need) noexcept {
    switch (need) {
    case NeedType::Hunger:
    case NeedType::Thirst:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::optional<std::pair<NeedType, genesis::world::ResourceType>> criticalNeedResource(
    const NeedComponent& component) {
    struct Candidate {
        NeedType need{NeedType::Hunger};
        genesis::world::ResourceType resource{genesis::world::ResourceType::Food};
        float severity{0.0f};
    };

    std::optional<Candidate> best;

    const auto consider = [&](NeedType need, genesis::world::ResourceType resource) {
        const auto* state = component.needs.state(need);
        const auto* descriptor = component.needs.descriptor(need);
        if (!state || !descriptor) {
            return;
        }
        if (state->value < descriptor->criticalThreshold) {
            return;
        }
        const float denom = std::max(1.0f, descriptor->maxValue - descriptor->criticalThreshold);
        const float severity = (state->value - descriptor->criticalThreshold) / denom;
        if (!best || severity > best->severity) {
            best = Candidate{need, resource, severity};
        }
    };

    consider(NeedType::Hunger, genesis::world::ResourceType::Food);
    consider(NeedType::Thirst, genesis::world::ResourceType::Water);
    consider(NeedType::Social, genesis::world::ResourceType::Social);

    if (!best) {
        return std::nullopt;
    }
    return std::make_pair(best->need, best->resource);
}

[[nodiscard]] std::uint32_t readPositiveU32Or(const nlohmann::json& obj, const char* key, std::uint32_t fallback) {
    if (!obj.contains(key)) {
        return std::max<std::uint32_t>(1U, fallback);
    }
    const auto& v = obj.at(key);
    if (v.is_number_unsigned()) {
        return std::max<std::uint32_t>(1U, v.get<std::uint32_t>());
    }
    if (v.is_number_integer()) {
        const auto i = v.get<std::int64_t>();
        if (i > 0) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return std::max<std::uint32_t>(1U, fallback);
}

[[nodiscard]] std::uint32_t readU32Or(const nlohmann::json& obj, const char* key, std::uint32_t fallback) {
    if (!obj.contains(key)) {
        return fallback;
    }
    const auto& v = obj.at(key);
    if (v.is_number_unsigned()) {
        return v.get<std::uint32_t>();
    }
    if (v.is_number_integer()) {
        const auto i = v.get<std::int64_t>();
        if (i >= 0 && i <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return fallback;
}

[[nodiscard]] std::string unreachableWithDetail(const char* detail) {
    std::string out(kReasonUnreachable);
    out.push_back(':');
    out.append(detail);
    return out;
}

[[nodiscard]] std::string reasonWithType(const char* prefix, genesis::world::ResourceType type, bool unreachable) {
    std::string out(prefix);
    out.push_back(':');
    out.append(genesis::world::resourceTypeName(type));
    if (unreachable) {
        out.append(":Unreachable");
    }
    return out;
}

[[nodiscard]] bool anyReachableResourceType(const genesis::world::WorldDatabase& db,
                                             genesis::world::MapId startMap,
                                             genesis::world::ResourceType type) {
    for (const auto& m : db.maps()) {
        for (const auto& it : db.interactions(m.id)) {
            if (it.kind != genesis::world::InteractionKind::Resource) {
                continue;
            }
            if (it.resourceType.value_or(genesis::world::ResourceType::Food) != type) {
                continue;
            }
            const auto path = genesis::world::shortestMapPath(db, startMap, it.mapId);
            if (path) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

namespace components {

struct WorkshopJob {
    genesis::world::InteractionId interaction{0};
    genesis::world::ResourceType outputType{genesis::world::ResourceType::Food};
    std::uint32_t plannedBatches{0};
    std::uint32_t outputUnitsPerBatch{1};
    std::uint32_t workTotalTicks{0};
    std::uint32_t workRemainingTicks{0};
    std::array<std::uint32_t, components::CarriedResources::kTypeCount> reservedConsumables{};
};

struct SocialJob {
    std::uint32_t partnerEntityId{0};
    std::uint32_t workTotalTicks{0};
    std::uint32_t workRemainingTicks{0};
    float relief{0.0f};
};

} // namespace components

namespace {

[[nodiscard]] genesis::world::ResourceType resourceTypeFromIndex(std::size_t idx) {
    switch (idx) {
    case 0:
        return genesis::world::ResourceType::Food;
    case 1:
        return genesis::world::ResourceType::Water;
    case 2:
        return genesis::world::ResourceType::Social;
    case 3:
        return genesis::world::ResourceType::Ore;
    case 4:
        return genesis::world::ResourceType::Tool;
    default:
        return genesis::world::ResourceType::Food;
    }
}

} // namespace

ActionExecutor::ActionExecutor(genesis::world::WorldDatabase& db, genesis::world::system::ResourceSystem& resources)
    : m_db(db)
    , m_resources(resources) {}

void ActionExecutor::drainWorkshopAttemptSnapshots(std::vector<telemetry::WorkshopAttemptSnapshot>& out) noexcept {
    out.clear();
    out.swap(m_workshopAttempts);
}

void ActionExecutor::drainResourceAttemptSnapshots(std::vector<telemetry::ResourceAttemptSnapshot>& out) noexcept {
    out.clear();
    out.swap(m_resourceAttempts);
}

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

void ActionExecutor::requestSocialize(entt::entity entity,
                                      std::uint32_t partnerEntityId,
                                      std::uint32_t workTicks,
                                      float relief,
                                      entt::registry& registry) {
    if (partnerEntityId == 0U) {
        return;
    }

    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (hasPendingSocialize(queue, partnerEntityId)) {
        return;
    }

    ActionTask socialize{};
    socialize.type = ActionType::SocializeWithAgent;
    socialize.targetEntityId = partnerEntityId;
    socialize.speed = 1.0f;
    socialize.need = NeedType::Social;
    socialize.resource = genesis::world::ResourceType::Social;
    socialize.amount = std::max<std::uint32_t>(1U, workTicks);
    socialize.reliefPerUnit = relief;
    queue.tasks.push_back(socialize);
}

void ActionExecutor::update(entt::registry& registry, float deltaSeconds) {
    m_workshopAttempts.clear();
    m_resourceAttempts.clear();

    auto view = registry.view<ActionQueue, components::AgentLocation2D>();

    for (auto entity : view) {
        auto& queue = view.get<ActionQueue>(entity);
        auto& location = view.get<components::AgentLocation2D>(entity);

        const auto findBestInteractionFor = [&](genesis::world::ResourceType type) -> genesis::world::InteractionId {
            genesis::world::InteractionId best = 0;
            float bestCost = std::numeric_limits<float>::infinity();

            m_resources.forEachSpawn(registry, [&](const auto& spawn, const auto& inventory) {
                if (spawn.type != type) {
                    return;
                }

                const auto inter = m_db.findInteraction(spawn.interaction);
                if (!inter) {
                    return;
                }

                float cost = 0.0f;
                if (inter->mapId == location.mapId) {
                    const auto coord = inter->worldCoord();
                    const float dx = static_cast<float>(coord.first) - location.x;
                    const float dy = static_cast<float>(coord.second) - location.y;
                    cost = dx * dx + dy * dy;
                } else {
                    const auto path = genesis::world::shortestMapPath(m_db, location.mapId, inter->mapId);
                    if (!path) {
                        return;
                    }
                    cost = 500.0f + static_cast<float>(path->totalCost);
                }

                // Prefer non-empty targets when possible, but still allow empty workshops.
                if (inventory.current == 0U) {
                    cost += 250.0f;
                }

                if (cost < bestCost) {
                    bestCost = cost;
                    best = spawn.interaction;
                }
            });

            return best;
        };

        const auto ensureCriticalConsume = [&](NeedType criticalNeed, genesis::world::ResourceType criticalResource, bool& inserted) -> bool {
            inserted = false;
            for (const auto& t : queue.tasks) {
                if (t.type == ActionType::ConsumeResource && t.need == criticalNeed) {
                    return true;
                }
            }

            const auto target = findBestInteractionFor(criticalResource);
            if (target == 0) {
                return false;
            }

            float buffer = 1.0f;
            if (const auto* exp = registry.try_get<components::AgentExperience>(entity)) {
                const auto idx = needIndex(criticalNeed);
                if (idx < exp->bufferMultiplier.size()) {
                    buffer = std::clamp(exp->bufferMultiplier[idx], 1.0f, 3.0f);
                }
            }

            std::uint32_t baseUnits = 1U;
            float reliefPerUnit = 12.0f;
            switch (criticalNeed) {
            case NeedType::Hunger:
                baseUnits = 2U;
                reliefPerUnit = 12.0f;
                break;
            case NeedType::Thirst:
                baseUnits = 2U;
                reliefPerUnit = 12.0f;
                break;
            case NeedType::Social:
                baseUnits = 1U;
                reliefPerUnit = 20.0f;
                break;
            default:
                baseUnits = 1U;
                reliefPerUnit = 12.0f;
                break;
            }

            const auto scaled = static_cast<std::uint32_t>(std::lround(static_cast<double>(baseUnits) * static_cast<double>(buffer)));

            ActionTask consume{};
            consume.type = ActionType::ConsumeResource;
            consume.interaction = target;
            consume.need = criticalNeed;
            consume.resource = criticalResource;
            consume.amount = std::max<std::uint32_t>(1U, scaled);
            consume.retries = 0;
            consume.reliefPerUnit = reliefPerUnit;
            queue.tasks.emplace(queue.tasks.begin(), consume);
            inserted = true;
            return true;
        };

        const auto abandonAllActions = [&]() {
            queue.tasks.clear();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
        };

        const auto abortWorkshopJob = [&](components::WorkshopJob& job, NeedType criticalNeed) {
            auto* carried = registry.try_get<components::CarriedResources>(entity);
            if (!carried) {
                carried = &registry.emplace<components::CarriedResources>(entity);
            }

            for (std::size_t idx = 0; idx < job.reservedConsumables.size(); ++idx) {
                const auto amount = job.reservedConsumables[idx];
                if (amount == 0U) {
                    continue;
                }
                carried->add(resourceTypeFromIndex(idx), amount);
            }

            telemetry::WorkshopAttemptSnapshot attempt{};
            attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
            attempt.interactionId = job.interaction;
            attempt.outputType = job.outputType;
            attempt.wantedBatches = std::max<std::uint32_t>(1U, job.plannedBatches);
            attempt.wantedUnits = std::max<std::uint32_t>(1U, job.outputUnitsPerBatch) * attempt.wantedBatches;
            attempt.producedUnits = 0U;
            attempt.failureReason = preemptedWithNeed(needTypeName(criticalNeed));
            m_workshopAttempts.push_back(std::move(attempt));

            releaseWorkshopSlot(job.interaction, entity);
            registry.remove<components::WorkshopJob>(entity);
            abandonAllActions();
        };

        if (auto* needs = registry.try_get<NeedComponent>(entity)) {
            if (auto critical = criticalNeedResource(*needs)) {
                const auto [criticalNeed, criticalResource] = *critical;
                if (!isVitalNeed(criticalNeed)) {
                    // Social "critical" should not preempt work globally; it is handled by normal planning/actions.
                } else {

                const auto alreadyHasCriticalConsume = [&]() -> bool {
                    for (const auto& t : queue.tasks) {
                        if (t.type == ActionType::ConsumeResource && t.need == criticalNeed) {
                            return true;
                        }
                    }
                    return false;
                };

                const auto preemptForCriticalNeed = [&](bool mustAbortWorkshop) {
                    bool inserted = false;
                    if (!ensureCriticalConsume(criticalNeed, criticalResource, inserted)) {
                        recordCriticalPreempt(entity, criticalNeed, registry);
                        if (mustAbortWorkshop) {
                            if (auto* job = registry.try_get<components::WorkshopJob>(entity)) {
                                abortWorkshopJob(*job, criticalNeed);
                                return;
                            }
                        }
                        abandonAllActions();
                        return;
                    }
                    if (inserted) {
                        recordCriticalPreempt(entity, criticalNeed, registry);
                    }
                };

                if (auto* job = registry.try_get<components::WorkshopJob>(entity)) {
                    if (job->outputType != criticalResource && !alreadyHasCriticalConsume()) {
                        // Take a break instead of throwing away progress: eat/drink first, then resume work.
                        releaseWorkshopSlot(job->interaction, entity);
                        preemptForCriticalNeed(true);
                    }
                } else if (!alreadyHasCriticalConsume()) {
                    // Any non-critical work (including socializing) yields to vital needs.
                    preemptForCriticalNeed(false);
                }

                }
            }
        }

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
                advanced = processProduce(entity, queue, location, registry);
                break;
            case ActionType::SocializeWithAgent:
                processSocialize(entity, queue, location, registry);
                advanced = false;
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

bool ActionExecutor::hasPendingSocialize(const ActionQueue& queue, std::uint32_t partnerEntityId) const {
    if (partnerEntityId == 0U) {
        return false;
    }
    return std::any_of(queue.tasks.begin(), queue.tasks.end(), [&](const ActionTask& task) {
        return task.type == ActionType::SocializeWithAgent && task.targetEntityId == partnerEntityId;
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
        telemetry::ResourceAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.action = "ConsumeResource";
        attempt.interactionId = task.interaction;
        attempt.resourceType = task.resource;
        attempt.wantedUnits = task.amount;
        attempt.failureReason = kReasonMissingInteraction;
        recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::MissingInteraction, registry);
        m_resourceAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        if (location.mapId != targetMap) {
            const auto path = genesis::world::shortestMapPath(m_db, location.mapId, targetMap);
            if (!path || path->maps.size() < 2) {
                telemetry::ResourceAttemptSnapshot attempt{};
                attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
                attempt.action = "ConsumeResource";
                attempt.interactionId = task.interaction;
                attempt.resourceType = task.resource;
                attempt.wantedUnits = task.amount;
                attempt.failureReason = unreachableWithDetail("NoPath");
                recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::Unreachable, registry);
                m_resourceAttempts.push_back(std::move(attempt));
                queue.tasks.pop_front();
                return;
            }

            const auto nextMap = path->maps[1];
            bool hasPortal = false;
            for (const auto& portal : m_db.portals(location.mapId)) {
                if (portal.targetMapId == nextMap) {
                    hasPortal = true;
                    break;
                }
            }
            if (!hasPortal) {
                telemetry::ResourceAttemptSnapshot attempt{};
                attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
                attempt.action = "ConsumeResource";
                attempt.interactionId = task.interaction;
                attempt.resourceType = task.resource;
                attempt.wantedUnits = task.amount;
                attempt.failureReason = unreachableWithDetail("NoPortal");
                recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::Unreachable, registry);
                m_resourceAttempts.push_back(std::move(attempt));
                queue.tasks.pop_front();
                return;
            }
        }

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
    telemetry::ResourceAttemptSnapshot attempt{};
    attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
    attempt.action = "ConsumeResource";
    attempt.interactionId = task.interaction;
    attempt.resourceType = task.resource;
    attempt.wantedUnits = task.amount;
    attempt.obtainedUnits = consumed;
    if (consumed > 0U) {
        const float relief = static_cast<float>(consumed) * task.reliefPerUnit;
        state->value = std::max(descriptor->minValue, state->value - relief);
        state->clamp(*descriptor);
        needs->lastSamples[needIndex(task.need)] = std::nullopt;
    }

    if (consumed == 0U) {
        auto* carried = registry.try_get<components::CarriedResources>(entity);
        if (!carried) {
            carried = &registry.emplace<components::CarriedResources>(entity);
        }

        ProductionPlanner planner(m_db);
        ProductionPlanner::Request req{};
        req.location = &location;
        req.carried = carried;
        req.targetWorkshop = task.interaction;
        req.outputType = task.resource;
        req.outputAmount = task.amount;
        req.currentRetries = task.retries;
        req.need = task.need;
        req.reliefPerUnit = task.reliefPerUnit;

        if (auto plan = planner.buildRecoveryPlan(req)) {
            attempt.failureReason = kReasonStockout;
            attempt.recoveryPlanned = true;
            recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::Stockout, registry);
            m_resourceAttempts.push_back(std::move(attempt));
            queue.tasks.pop_front();
            queue.tasks.insert(queue.tasks.begin(), plan->begin(), plan->end());
            return;
        }

        attempt.failureReason = kReasonPlanningFailed;
        recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::PlanningFailed, registry);
        m_resourceAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return;
    }

    recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::None, registry);
    m_resourceAttempts.push_back(std::move(attempt));
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
        telemetry::ResourceAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.action = "TakeResource";
        attempt.interactionId = task.interaction;
        attempt.resourceType = task.resource;
        attempt.wantedUnits = task.amount;
        attempt.failureReason = kReasonMissingInteraction;
        recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::MissingInteraction, registry);
        m_resourceAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        if (location.mapId != targetMap) {
            const auto path = genesis::world::shortestMapPath(m_db, location.mapId, targetMap);
            if (!path || path->maps.size() < 2) {
                telemetry::ResourceAttemptSnapshot attempt{};
                attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
                attempt.action = "TakeResource";
                attempt.interactionId = task.interaction;
                attempt.resourceType = task.resource;
                attempt.wantedUnits = task.amount;
                attempt.failureReason = unreachableWithDetail("NoPath");
                recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::Unreachable, registry);
                m_resourceAttempts.push_back(std::move(attempt));
                queue.tasks.pop_front();
                return;
            }

            const auto nextMap = path->maps[1];
            bool hasPortal = false;
            for (const auto& portal : m_db.portals(location.mapId)) {
                if (portal.targetMapId == nextMap) {
                    hasPortal = true;
                    break;
                }
            }
            if (!hasPortal) {
                telemetry::ResourceAttemptSnapshot attempt{};
                attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
                attempt.action = "TakeResource";
                attempt.interactionId = task.interaction;
                attempt.resourceType = task.resource;
                attempt.wantedUnits = task.amount;
                attempt.failureReason = unreachableWithDetail("NoPortal");
                recordResourceAttempt(entity, task, attempt, ResourceAttemptFailure::Unreachable, registry);
                m_resourceAttempts.push_back(std::move(attempt));
                queue.tasks.pop_front();
                return;
            }
        }

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
    telemetry::ResourceAttemptSnapshot attempt{};
    attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
    attempt.action = "TakeResource";
    attempt.interactionId = task.interaction;
    attempt.resourceType = task.resource;
    attempt.wantedUnits = want;
    attempt.obtainedUnits = taken;
    ResourceAttemptFailure failure = ResourceAttemptFailure::None;
    if (taken == 0U) {
        attempt.failureReason = kReasonStockout;
        failure = ResourceAttemptFailure::Stockout;
    }
    recordResourceAttempt(entity, task, attempt, failure, registry);
    m_resourceAttempts.push_back(std::move(attempt));
    carried->add(task.resource, taken);
    queue.tasks.pop_front();
}

bool ActionExecutor::processProduce(entt::entity entity,
                                   ActionQueue& queue,
                                   components::AgentLocation2D& location,
                                   entt::registry& registry) {
    if (queue.tasks.empty()) {
        return true;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = std::max<std::uint32_t>(1U, task.batches);
        attempt.failureReason = kReasonMissingInteraction;
        m_workshopAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return true;
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
        return false;
    }

    std::uint32_t workTicksPerBatch = 1U;
    std::uint32_t slots = 0U;
    if (it->meta && it->meta->contains("workshop") && (*it->meta)["workshop"].is_object()) {
        const auto& metaWorkshop = (*it->meta)["workshop"];
        workTicksPerBatch = readPositiveU32Or(metaWorkshop, "workTicksPerBatch", 1U);
        slots = readU32Or(metaWorkshop, "slots", 0U);
    }

    if (auto* job = registry.try_get<components::WorkshopJob>(entity)) {
        if (job->interaction != task.interaction || job->outputType != task.resource) {
            releaseWorkshopSlot(job->interaction, entity);
            registry.remove<components::WorkshopJob>(entity);
            queue.tasks.pop_front();
            return true;
        }

        if (!acquireWorkshopSlot(job->interaction, entity, slots)) {
            return false;
        }

        if (job->workRemainingTicks > 0U) {
            job->workRemainingTicks--;
        }
        if (job->workRemainingTicks > 0U) {
            return false;
        }

        const std::uint32_t plannedBatches = std::max<std::uint32_t>(1U, job->plannedBatches);
        const std::uint32_t outputUnitsPerBatch = std::max<std::uint32_t>(1U, job->outputUnitsPerBatch);
        const std::uint32_t wantedUnits = outputUnitsPerBatch * plannedBatches;
        const std::uint32_t producedUnits = m_resources.produceAtInteraction(registry, task.interaction, wantedUnits);

        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = std::max<std::uint32_t>(1U, task.batches);
        attempt.wantedUnits = outputUnitsPerBatch * attempt.wantedBatches;
        attempt.producedUnits = producedUnits;
        if (producedUnits == 0U) {
            attempt.failureReason = kReasonOutputFull;
        }
        m_workshopAttempts.push_back(std::move(attempt));

        releaseWorkshopSlot(job->interaction, entity);
        registry.remove<components::WorkshopJob>(entity);
        queue.tasks.pop_front();
        return true;
    }

    const auto recipes = parseWorkshopRecipes(*it);
    if (recipes.empty()) {
        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = std::max<std::uint32_t>(1U, task.batches);
        attempt.failureReason = kReasonNoRecipes;
        m_workshopAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return true;
    }

    auto* carried = registry.try_get<components::CarriedResources>(entity);
    if (!carried) {
        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = std::max<std::uint32_t>(1U, task.batches);
        attempt.failureReason = kReasonNoCarriedResources;
        m_workshopAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return true;
    }

    const auto state = m_resources.spawnState(registry, task.interaction);
    if (!state) {
        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = std::max<std::uint32_t>(1U, task.batches);
        attempt.failureReason = kReasonNoSpawnState;
        m_workshopAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return true;
    }

    const auto wantedBatches = std::max<std::uint32_t>(1U, task.batches);

    const WorkshopRecipe* bestRecipe = nullptr;
    std::uint32_t bestBatches = 0U;

    bool anyCapacity = false;
    bool anyNonConsumableOk = false;
    bool anyConsumableOk = false;

    std::unordered_map<std::uint32_t, std::uint32_t> missingNonConsumableByType;
    std::unordered_map<std::uint32_t, std::uint32_t> missingConsumableByType;

    for (const auto& r : recipes) {
        const auto outputUnits = std::max<std::uint32_t>(1U, r.outputUnits);

        std::uint32_t maxByInputs = std::numeric_limits<std::uint32_t>::max();
        bool nonConsumablesOk = true;
        for (const auto& input : r.inputs) {
            if (input.units == 0U) {
                continue;
            }
            if (input.consumable) {
                maxByInputs = std::min(maxByInputs, carried->get(input.type) / input.units);
            } else {
                if (carried->get(input.type) < input.units) {
                    nonConsumablesOk = false;
                }
            }
        }

        const std::uint32_t space = (state->current >= state->capacity) ? 0U : (state->capacity - state->current);
        const std::uint32_t maxByCapacity = space / outputUnits;

        anyCapacity = anyCapacity || (maxByCapacity > 0U);
        anyNonConsumableOk = anyNonConsumableOk || nonConsumablesOk;

        const std::uint32_t effectiveByInputs = nonConsumablesOk ? maxByInputs : 0U;
        anyConsumableOk = anyConsumableOk || (nonConsumablesOk && effectiveByInputs > 0U);

        if (maxByCapacity > 0U) {
            if (!nonConsumablesOk) {
                for (const auto& input : r.inputs) {
                    if (input.units == 0U || input.consumable) {
                        continue;
                    }
                    if (carried->get(input.type) < input.units) {
                        missingNonConsumableByType[static_cast<std::uint32_t>(input.type)]++;
                    }
                }
            } else if (effectiveByInputs == 0U) {
                for (const auto& input : r.inputs) {
                    if (input.units == 0U || !input.consumable) {
                        continue;
                    }
                    if (carried->get(input.type) < input.units) {
                        missingConsumableByType[static_cast<std::uint32_t>(input.type)]++;
                    }
                }
            }
        }

        const std::uint32_t batches = std::min(wantedBatches, std::min(effectiveByInputs, maxByCapacity));
        if (batches > bestBatches) {
            bestBatches = batches;
            bestRecipe = &r;
        }
    }

    if (!bestRecipe || bestBatches == 0U) {
        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = wantedBatches;
        if (!anyCapacity) {
            attempt.failureReason = kReasonOutputFull;
        } else if (!anyNonConsumableOk && !missingNonConsumableByType.empty()) {
            std::uint32_t bestType = missingNonConsumableByType.begin()->first;
            std::uint32_t bestCount = missingNonConsumableByType.begin()->second;
            for (const auto& [t, c] : missingNonConsumableByType) {
                if (c > bestCount) {
                    bestType = t;
                    bestCount = c;
                }
            }
            const auto type = static_cast<genesis::world::ResourceType>(bestType);
            const bool reachable = anyReachableResourceType(m_db, location.mapId, type);
            attempt.failureReason = reasonWithType(kReasonMissingNonConsumableInput, type, !reachable);
        } else if (!anyConsumableOk && !missingConsumableByType.empty()) {
            std::uint32_t bestType = missingConsumableByType.begin()->first;
            std::uint32_t bestCount = missingConsumableByType.begin()->second;
            for (const auto& [t, c] : missingConsumableByType) {
                if (c > bestCount) {
                    bestType = t;
                    bestCount = c;
                }
            }
            const auto type = static_cast<genesis::world::ResourceType>(bestType);
            const bool reachable = anyReachableResourceType(m_db, location.mapId, type);
            attempt.failureReason = reasonWithType(kReasonMissingConsumableInput, type, !reachable);
        } else {
            attempt.failureReason = kReasonUnknown;
        }
        m_workshopAttempts.push_back(std::move(attempt));
        queue.tasks.pop_front();
        return true;
    }

    if (!acquireWorkshopSlot(task.interaction, entity, slots)) {
        return false;
    }

    const auto outputUnits = std::max<std::uint32_t>(1U, bestRecipe->outputUnits);

    if (workTicksPerBatch <= 1U) {
        for (const auto& input : bestRecipe->inputs) {
            if (input.consumable) {
                carried->remove(input.type, input.units * bestBatches);
            }
        }

        const std::uint32_t wantedUnits = outputUnits * bestBatches;
        const std::uint32_t producedUnits = m_resources.produceAtInteraction(registry, task.interaction, wantedUnits);

        telemetry::WorkshopAttemptSnapshot attempt{};
        attempt.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        attempt.interactionId = task.interaction;
        attempt.outputType = task.resource;
        attempt.wantedBatches = wantedBatches;
        attempt.wantedUnits = outputUnits * wantedBatches;
        attempt.producedUnits = producedUnits;
        if (producedUnits == 0U) {
            attempt.failureReason = kReasonOutputFull;
        }
        m_workshopAttempts.push_back(std::move(attempt));

        releaseWorkshopSlot(task.interaction, entity);
        queue.tasks.pop_front();
        return true;
    }

    components::WorkshopJob job{};
    job.interaction = task.interaction;
    job.outputType = task.resource;
    job.plannedBatches = bestBatches;
    job.outputUnitsPerBatch = outputUnits;
    job.workTotalTicks = workTicksPerBatch * std::max<std::uint32_t>(1U, bestBatches);
    job.workRemainingTicks = job.workTotalTicks;
    for (const auto& input : bestRecipe->inputs) {
        if (!input.consumable) {
            continue;
        }
        const std::uint32_t used = input.units * bestBatches;
        const auto removed = carried->remove(input.type, used);
        const auto idx = components::resourceTypeIndex(input.type);
        if (idx < job.reservedConsumables.size()) {
            job.reservedConsumables[idx] += removed;
        }
    }

    registry.emplace_or_replace<components::WorkshopJob>(entity, std::move(job));
    auto& active = registry.get<components::WorkshopJob>(entity);
    if (active.workRemainingTicks > 0U) {
        active.workRemainingTicks--;
    }
    return active.workRemainingTicks == 0U;
}

void ActionExecutor::processSocialize(entt::entity entity,
                                      ActionQueue& queue,
                                      components::AgentLocation2D& location,
                                      entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    const auto partner = static_cast<entt::entity>(task.targetEntityId);
    if (partner == entt::null || !registry.valid(partner) || !registry.all_of<components::AgentLocation2D>(partner)) {
        queue.tasks.pop_front();
        if (registry.any_of<components::SocialJob>(entity)) {
            registry.remove<components::SocialJob>(entity);
        }
        return;
    }

    const auto& partnerLoc = registry.get<components::AgentLocation2D>(partner);
    if (partnerLoc.mapId != location.mapId) {
        queue.tasks.pop_front();
        if (registry.any_of<components::SocialJob>(entity)) {
            registry.remove<components::SocialJob>(entity);
        }
        return;
    }

    const float dx = partnerLoc.x - location.x;
    const float dy = partnerLoc.y - location.y;
    const float dist2 = dx * dx + dy * dy;
    constexpr float kMeetEpsilon2 = 0.05f * 0.05f;
    if (dist2 > kMeetEpsilon2) {
        auto& intent = registry.get_or_emplace<components::MovementIntent2D>(entity);
        intent.targetMapId = location.mapId;
        intent.targetX = partnerLoc.x;
        intent.targetY = partnerLoc.y;
        intent.speed = std::max(task.speed, kMinSpeed);
        return;
    }

    auto& job = registry.get_or_emplace<components::SocialJob>(entity);
    if (job.partnerEntityId != task.targetEntityId || job.workTotalTicks == 0U) {
        job.partnerEntityId = task.targetEntityId;
        job.workTotalTicks = std::max<std::uint32_t>(1U, task.amount);
        job.workRemainingTicks = job.workTotalTicks;
        job.relief = task.reliefPerUnit;
    }

    if (job.workRemainingTicks > 0U) {
        --job.workRemainingTicks;
        return;
    }

    auto* needs = registry.try_get<NeedComponent>(entity);
    if (needs) {
        auto* state = needs->needs.state(NeedType::Social);
        const auto* descriptor = needs->needs.descriptor(NeedType::Social);
        if (state && descriptor) {
            state->value = std::max(descriptor->minValue, state->value - std::max(0.0f, job.relief));
            state->clamp(*descriptor);
            needs->lastSamples[needIndex(NeedType::Social)] = std::nullopt;
        }
    }

    // Co-located social contact benefits both sides (even if only one initiated).
    if (registry.valid(partner) && registry.all_of<components::AgentLocation2D>(partner)) {
        const auto& pLoc = registry.get<components::AgentLocation2D>(partner);
        const float pdx = pLoc.x - location.x;
        const float pdy = pLoc.y - location.y;
        const float pd2 = pdx * pdx + pdy * pdy;
        if (pLoc.mapId == location.mapId && pd2 <= kMeetEpsilon2) {
            if (auto* pNeeds = registry.try_get<NeedComponent>(partner)) {
                auto* pState = pNeeds->needs.state(NeedType::Social);
                const auto* pDesc = pNeeds->needs.descriptor(NeedType::Social);
                if (pState && pDesc) {
                    pState->value = std::max(pDesc->minValue, pState->value - std::max(0.0f, job.relief));
                    pState->clamp(*pDesc);
                    pNeeds->lastSamples[needIndex(NeedType::Social)] = std::nullopt;
                }
            }
        }
    }

    registry.remove<components::SocialJob>(entity);
    queue.tasks.pop_front();
}

bool ActionExecutor::acquireWorkshopSlot(genesis::world::InteractionId interaction, entt::entity worker, std::uint32_t slots) {
    if (slots == 0U) {
        return true;
    }
    slots = std::max<std::uint32_t>(1U, slots);
    auto& workers = m_workshopWorkers[interaction];
    if (std::find(workers.begin(), workers.end(), worker) != workers.end()) {
        return true;
    }
    if (workers.size() >= static_cast<std::size_t>(slots)) {
        return false;
    }
    workers.push_back(worker);
    return true;
}

void ActionExecutor::releaseWorkshopSlot(genesis::world::InteractionId interaction, entt::entity worker) {
    auto it = m_workshopWorkers.find(interaction);
    if (it == m_workshopWorkers.end()) {
        return;
    }
    auto& workers = it->second;
    workers.erase(std::remove(workers.begin(), workers.end(), worker), workers.end());
    if (workers.empty()) {
        m_workshopWorkers.erase(it);
    }
}

} // namespace genesis::agents
