#include "systems/ActionExecutionSystem.h"

void ActionExecutionSystem::update(entt::registry& registry, float deltaTime, const ConfigManager& configManager) {
    // 查询所有有行动队列的实体
    auto view = registry.view<ActionQueueComponent, PositionComponent>();

    for (auto entity : view) {
        auto& actionQueue = view.get<ActionQueueComponent>(entity);
        const auto& position = view.get<PositionComponent>(entity);

        // 如果行动队列为空，重置目标并跳过
        if (actionQueue.isEmpty()) {
            resetGoal(registry, entity);
            continue;
        }

        // 获取当前要执行的行动
        Action currentAction = actionQueue.getNextAction();

        // 执行行动
        bool actionCompleted = false;
        switch (currentAction.type) {
            case ActionType::MOVE_TO:
                actionCompleted = executeMoveTo(registry, entity, currentAction.targetEntity, position);
                break;
            case ActionType::CONSUME:
                actionCompleted = executeConsume(registry, entity, currentAction.targetEntity, configManager);
                break;
            case ActionType::CHAT_WITH:
                actionCompleted = executeChatWith(registry, entity, currentAction.targetEntity);
                break;
            case ActionType::GO_HOME_AND_REST:
                actionCompleted = executeGoHomeAndRest(registry, entity, currentAction.targetEntity);
                break;
            case ActionType::FIND_SOCIAL_TARGET_IN_ZONE:
                actionCompleted = executeFindSocialTargetInZone(registry, entity, currentAction.targetEntity);
                break;
            default:
                actionCompleted = true;  // 未知行动，直接完成
                break;
        }

        // 如果行动完成，从队列中移除
        if (actionCompleted) {
            actionQueue.completeCurrentAction();
            std::cout << "Entity " << static_cast<uint32_t>(entity)
              << " completed action (" << actionQueue.actions.size() << " left)" << std::endl;
        }
    }
}

bool ActionExecutionSystem::executeMoveTo(entt::registry& registry, entt::entity entity,
                                         entt::entity target, const PositionComponent& currentPos) {
    // 检查目标是否存在且有效
    if (!registry.valid(target) || !registry.all_of<PositionComponent>(target)) {
        std::cout << "Target entity " << static_cast<uint32_t>(target)
                  << " is no longer valid!" << std::endl;
        return true;  // 行动失败，但标记为完成
    }

    const auto& targetPos = registry.get<PositionComponent>(target);

    // 计算到目标的距离
    float dx = targetPos.x - currentPos.x;
    float dy = targetPos.y - currentPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);

    // 如果已经足够接近目标
    if (distance <= ARRIVAL_THRESHOLD) {
        std::cout << "Entity " << static_cast<uint32_t>(entity) << " arrived" << std::endl;
        return true;  // 移动完成
    }

    // 确保实体有速度组件
    if (!registry.all_of<VelocityComponent>(entity)) {
        registry.emplace<VelocityComponent>(entity);
    }

    auto& velocity = registry.get<VelocityComponent>(entity);

    // 计算移动方向（标准化向量）
    velocity.vx = (dx / distance) * MOVE_SPEED;
    velocity.vy = (dy / distance) * MOVE_SPEED;

    // 静默移动处理（移除冗余输出）

    return false;  // 移动尚未完成
}

bool ActionExecutionSystem::executeConsume(entt::registry& registry, entt::entity consumer, entt::entity food, const ConfigManager& configManager) {
    // 检查食物是否仍然存在
    if (!registry.valid(food)) {
        std::cout << "Food entity " << static_cast<uint32_t>(food)
                  << " no longer exists!" << std::endl;
        return true;
    }

    // 检查食物是否有食物组件
    if (!registry.all_of<FoodComponent>(food)) {
        std::cout << "Target entity " << static_cast<uint32_t>(food)
                  << " is not food!" << std::endl;
        return true;
    }

    // 使用NeedsSystem来处理食物消费
    NeedsSystem::consumeFood(registry, consumer, food, configManager);

    return true;  // 消费行动完成
}

bool ActionExecutionSystem::executeChatWith(entt::registry& registry, entt::entity actor, entt::entity target) {
    // 检查目标是否存在且是Actor
    if (!registry.valid(target) || !registry.all_of<ActorComponent>(target)) {
        std::cout << "Chat target " << static_cast<uint32_t>(target)
                  << " is no longer valid!" << std::endl;
        return true;
    }

    // 确保双方都有必要的组件
    ensureSocialComponents(registry, actor);
    ensureSocialComponents(registry, target);

    // 获取身份信息用于日志
    std::string actorName = "Entity " + std::to_string(static_cast<uint32_t>(actor));
    std::string targetName = "Entity " + std::to_string(static_cast<uint32_t>(target));

    if (registry.all_of<IdentityComponent>(actor)) {
        const auto& identity = registry.get<IdentityComponent>(actor);
        actorName = identity.name;
    }

    if (registry.all_of<IdentityComponent>(target)) {
        const auto& identity = registry.get<IdentityComponent>(target);
        targetName = identity.name;
    }

    // 满足社交需求
    if (registry.all_of<MaslowNeedsComponent>(actor)) {
        auto& actorNeeds = registry.get<MaslowNeedsComponent>(actor);
        actorNeeds.loveBelonging = std::max(0.0f, actorNeeds.loveBelonging - 0.3f);
        std::cout << actorName << " feels less lonely after chatting with " << targetName << std::endl;
    }

    if (registry.all_of<MaslowNeedsComponent>(target)) {
        auto& targetNeeds = registry.get<MaslowNeedsComponent>(target);
        targetNeeds.loveBelonging = std::max(0.0f, targetNeeds.loveBelonging - 0.2f);
        std::cout << targetName << " also enjoys the conversation" << std::endl;
    }

    // 改善关系
    if (registry.all_of<RelationshipComponent>(actor)) {
        auto& actorRels = registry.get<RelationshipComponent>(actor);
        actorRels.improveRelationship(target, 2.0f);
    }

    if (registry.all_of<RelationshipComponent>(target)) {
        auto& targetRels = registry.get<RelationshipComponent>(target);
        targetRels.improveRelationship(actor, 1.5f);
    }

    // 交换知识
    exchangeKnowledge(registry, actor, target);

    std::cout << actorName << " had a pleasant conversation with " << targetName << std::endl;

    return true;  // 交谈行动完成
}

bool ActionExecutionSystem::executeGoHomeAndRest(entt::registry& registry, entt::entity actor, entt::entity homeTarget) {
    // 获取实体名称用于日志
    std::string actorName = "Entity " + std::to_string(static_cast<uint32_t>(actor));
    if (registry.all_of<IdentityComponent>(actor)) {
        const auto& identity = registry.get<IdentityComponent>(actor);
        actorName = identity.name;
    }

    // 检查是否有马斯洛需求组件
    if (!registry.all_of<MaslowNeedsComponent>(actor)) {
        std::cout << actorName << " cannot rest - no needs component" << std::endl;
        return true;
    }

    auto& needs = registry.get<MaslowNeedsComponent>(actor);

    // 如果疲劳度已经很低，休息完成
    if (needs.fatigue <= 0.1f) {
        std::cout << actorName << " is fully rested" << std::endl;
        return true;
    }

    // 检查是否在家（如果指定了家目标）
    if (registry.valid(homeTarget) && registry.all_of<PositionComponent>(homeTarget)) {
        const auto& homePos = registry.get<PositionComponent>(homeTarget);
        if (registry.all_of<PositionComponent>(actor)) {
            const auto& actorPos = registry.get<PositionComponent>(actor);
            float dx = homePos.x - actorPos.x;
            float dy = homePos.y - actorPos.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            // 如果还没到家，先移动到家
            if (distance > ARRIVAL_THRESHOLD) {
                std::cout << actorName << " is moving home to rest" << std::endl;
                return executeMoveTo(registry, actor, homeTarget, actorPos);
            }
        }
    }

    // 停止移动并开始休息
    if (registry.all_of<VelocityComponent>(actor)) {
        auto& velocity = registry.get<VelocityComponent>(actor);
        velocity.vx = 0.0f;
        velocity.vy = 0.0f;
    }

    // 恢复疲劳度
    needs.fatigue = std::max(0.0f, needs.fatigue - REST_RATE);

    std::cout << actorName << " is resting... Fatigue: "
              << static_cast<int>(needs.fatigue * 100) << "%" << std::endl;

    // 当疲劳度足够低时，休息完成
    if (needs.fatigue <= 0.1f) {
        std::cout << actorName << " finished resting and feels refreshed!" << std::endl;
        return true;
    }

    return false; // 休息还在进行中
}

bool ActionExecutionSystem::executeFindSocialTargetInZone(entt::registry& registry, entt::entity actor, entt::entity zoneEntity) {
    // 获取实体名称
    std::string actorName = "Entity " + std::to_string(static_cast<uint32_t>(actor));
    if (registry.all_of<IdentityComponent>(actor)) {
        const auto& identity = registry.get<IdentityComponent>(actor);
        actorName = identity.name;
    }

    // 检查区域实体是否有效
    if (!registry.valid(zoneEntity) || !registry.all_of<ZoneComponent>(zoneEntity)) {
        std::cout << actorName << " cannot find social targets - invalid zone" << std::endl;
        return true;
    }

    const auto& zone = registry.get<ZoneComponent>(zoneEntity);
    std::cout << actorName << " is looking for social targets in " << zone.name << std::endl;

    // 在区域内寻找其他Actor
    auto potentialTargets = registry.view<ActorComponent, PositionComponent>();
    for (auto target : potentialTargets) {
        // 跳过自己
        if (target == actor) continue;

        // 检查目标是否在区域内
        const auto& targetPos = potentialTargets.get<PositionComponent>(target);
        if (!zone.contains(targetPos.x, targetPos.y)) continue;

        // 找到了一个合适的目标
        std::string targetName = "Entity " + std::to_string(static_cast<uint32_t>(target));
        if (registry.all_of<IdentityComponent>(target)) {
            const auto& identity = registry.get<IdentityComponent>(target);
            targetName = identity.name;
        }

        std::cout << actorName << " found " << targetName << " in " << zone.name << std::endl;

        // 添加一个CHAT_WITH行动到队列，目标为找到的实体
        if (registry.all_of<ActionQueueComponent>(actor)) {
            auto& actionQueue = registry.get<ActionQueueComponent>(actor);
            // 在队列前面插入交谈行动
            actionQueue.actions.insert(actionQueue.actions.begin(),
                Action{ActionType::CHAT_WITH, target});
        }

        return true; // 寻找社交目标完成
    }

    // 没有找到合适的社交目标
    std::cout << actorName << " couldn't find anyone to socialize with in " << zone.name << std::endl;
    return true;
}

void ActionExecutionSystem::ensureSocialComponents(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<IdentityComponent>(entity)) {
        registry.emplace<IdentityComponent>(entity, "Unnamed " + std::to_string(static_cast<uint32_t>(entity)));
    }

    if (!registry.all_of<RelationshipComponent>(entity)) {
        registry.emplace<RelationshipComponent>(entity);
    }

    if (!registry.all_of<KnowledgeComponent>(entity)) {
        registry.emplace<KnowledgeComponent>(entity);
    }
}

void ActionExecutionSystem::exchangeKnowledge(entt::registry& registry, entt::entity actor, entt::entity target) {
    if (!registry.all_of<KnowledgeComponent>(actor) || !registry.all_of<KnowledgeComponent>(target)) {
        return;  // 其中一方没有知识组件，无法交换
    }

    auto& actorKnowledge = registry.get<KnowledgeComponent>(actor);
    auto& targetKnowledge = registry.get<KnowledgeComponent>(target);

    // Actor分享一个事实给Target
    const Fact* actorFact = actorKnowledge.getRandomFact();
    if (actorFact != nullptr) {
        // 创建事实副本，降低置信度（信息失真）
        Fact sharedFact = *actorFact;
        sharedFact.source = actor; // 设置来源为当前说话者
        sharedFact.confidence = std::max(0.1f, sharedFact.confidence * 0.9f); // 降低10%置信度

        targetKnowledge.addFact(sharedFact);

        std::string actorName = "Entity " + std::to_string(static_cast<uint32_t>(actor));
        if (registry.all_of<IdentityComponent>(actor)) {
            const auto& identity = registry.get<IdentityComponent>(actor);
            actorName = identity.name;
        }

        std::cout << actorName << " shared: \"" << sharedFact.content
                  << "\" (confidence: " << static_cast<int>(sharedFact.confidence * 100) << "%)" << std::endl;
    } else {
        // 如果Actor没有知识，创建一个默认的
        std::string actorName = "Entity " + std::to_string(static_cast<uint32_t>(actor));
        if (registry.all_of<IdentityComponent>(actor)) {
            const auto& identity = registry.get<IdentityComponent>(actor);
            actorName = identity.name;
        }

        Fact newFact("I am " + actorName, actor, 1.0f);
        targetKnowledge.addFact(newFact);
        std::cout << actorName << " introduced themselves to the conversation" << std::endl;
    }
}

void ActionExecutionSystem::resetGoal(entt::registry& registry, entt::entity entity) {
    // 重置目标组件
    if (registry.all_of<GoalComponent>(entity)) {
        auto& goal = registry.get<GoalComponent>(entity);
        goal.currentGoal = GoalType::NONE;
        goal.description = "No goal";
        goal.targetEntity = entt::null;
    }

    // 停止移动
    if (registry.all_of<VelocityComponent>(entity)) {
        auto& velocity = registry.get<VelocityComponent>(entity);
        velocity.vx = 0.0f;
        velocity.vy = 0.0f;
    }
}