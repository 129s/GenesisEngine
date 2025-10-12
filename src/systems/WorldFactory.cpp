#include "systems/WorldFactory.h"
#include "components/PositionComponent.h"
#include "components/RenderableComponent.h"
#include "components/ActorComponent.h"
#include "components/VelocityComponent.h"
#include "components/MaslowNeedsComponent.h"
#include "components/FoodComponent.h"
#include "components/IdentityComponent.h"
#include "components/KnowledgeComponent.h"
#include "components/RelationshipComponent.h"
#include "components/OwnershipComponent.h"
#include "components/ZoneComponent.h"
#include "systems/ConfigManager.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WorldFactory::WorldFactory(const ConfigManager& configManager)
    : config_manager_(configManager) {
}

bool WorldFactory::loadEntityTemplates(const std::string& templatesPath) {
    templates_path_ = templatesPath;

    std::ifstream file(templatesPath);
    if (!file.is_open()) {
        std::cerr << "无法打开实体模板文件: " << templatesPath << std::endl;
        return false;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();

    bool success = parseEntityTemplates(jsonContent);
    if (success) {
        templates_loaded_ = true;
        std::cout << "成功加载 " << templates_.size() << " 个实体模板" << std::endl;
    } else {
        std::cerr << "解析实体模板文件失败" << std::endl;
    }

    return success;
}

void WorldFactory::createWorld(entt::registry& registry) {
    if (!templates_loaded_) {
        std::cerr << "警告: 实体模板未加载，使用默认实体创建" << std::endl;
        createDefaultWorld(registry);
        return;
    }

    // 创建食物实体
    createFoodEntities(registry);

    // 创建区域实体
    createZoneEntities(registry);

    // 创建Actor实体
    createActorEntities(registry);

    std::cout << "世界创建完成" << std::endl;
}

void WorldFactory::createFoodEntities(entt::registry& registry) {
    // 从配置中获取食物配置 - 需要解析新的 world_config 结构
    // 这里暂时使用模板查找方式

    std::vector<std::pair<std::string, int>> foodTemplates = {
        {"BasicFoodTemplate", 3},
        {"FruitTemplate", 2},
        {"VegetableTemplate", 2},
        {"MeatTemplate", 1},
        {"BreadTemplate", 2},
        {"DairyTemplate", 1}
    };

    int totalFoodCount = 0;
    for (const auto& [templateName, count] : foodTemplates) {
        const EntityTemplate* foodTemplate = getTemplate(templateName);
        if (!foodTemplate) {
            std::cerr << "警告: 未找到 " << templateName << "，跳过" << std::endl;
            continue;
        }

        for (int i = 0; i < count; i++) {
            auto food = createEntityFromTemplate(registry, templateName);
            if (food != entt::null) {
                totalFoodCount++;
            }
        }
    }

    std::cout << "总计创建了 " << totalFoodCount << " 个食物实体" << std::endl;
}

void WorldFactory::createZoneEntities(entt::registry& registry) {
    // 尝试使用配置文件中的模板
    const EntityTemplate* tavernTemplate = getTemplate("TavernTemplate");
    const EntityTemplate* marketTemplate = getTemplate("MarketTemplate");
    const EntityTemplate* residenceTemplate = getTemplate("ResidenceTemplate");

    // 创建住宅区域
    std::vector<std::pair<std::string, std::pair<int, int>>> residences = {
        {"Alice的家", {5, 5}},
        {"Bob的家", {25, 5}},
        {"Charlie的家", {5, 25}},
        {"Diana的家", {25, 25}},
        {"Eve的家", {15, 35}}
    };

    for (const auto& [name, pos] : residences) {
        if (residenceTemplate) {
            auto zone = createZoneEntity(registry, *residenceTemplate);
            // 更新位置和名称
            if (registry.all_of<PositionComponent>(zone)) {
                auto& position = registry.get<PositionComponent>(zone);
                position.x = pos.first;
                position.y = pos.second;
            }
            if (registry.all_of<ZoneComponent>(zone)) {
                auto& zoneComp = registry.get<ZoneComponent>(zone);
                zoneComp.name = name;
                zoneComp.x = pos.first;
                zoneComp.y = pos.second;
            }
            std::cout << "Created residence zone from template: " << name << " at (" << pos.first << "," << pos.second << ")" << std::endl;
        } else {
            // 回退到硬编码创建
            auto zone = registry.create();
            registry.emplace<PositionComponent>(zone, pos.first, pos.second);
            registry.emplace<ZoneComponent>(zone, ZoneType::RESIDENCE, name, pos.first, pos.second,
                                          config_manager_.getWorldConfig().zones.residence.width,
                                          config_manager_.getWorldConfig().zones.residence.height);
            std::cout << "Created residence zone (fallback): " << name << " at (" << pos.first << "," << pos.second << ")" << std::endl;
        }
    }

    // 创建酒馆区域
    if (tavernTemplate) {
        auto tavern = createZoneEntity(registry, *tavernTemplate);
        std::cout << "Created tavern zone from template: 金色酒馆" << std::endl;
    } else {
        auto tavern = registry.create();
        registry.emplace<PositionComponent>(tavern, 15, 10);
        registry.emplace<ZoneComponent>(tavern, ZoneType::TAVERN, "金色酒馆", 15, 10,
                                       config_manager_.getWorldConfig().zones.tavern.width,
                                       config_manager_.getWorldConfig().zones.tavern.height);
        std::cout << "Created tavern zone (fallback): 金色酒馆 at (15,10)" << std::endl;
    }

    // 创建市场区域
    if (marketTemplate) {
        auto market = createZoneEntity(registry, *marketTemplate);
        std::cout << "Created market zone from template: 中央市场" << std::endl;
    } else {
        auto market = registry.create();
        registry.emplace<PositionComponent>(market, 30, 15);
        registry.emplace<ZoneComponent>(market, ZoneType::MARKET, "中央市场", 30, 15,
                                       config_manager_.getWorldConfig().zones.market.width,
                                       config_manager_.getWorldConfig().zones.market.height);
        std::cout << "Created market zone (fallback): 中央市场 at (30,15)" << std::endl;
    }
}

void WorldFactory::createActorEntities(entt::registry& registry) {
    std::vector<std::string> actorNames = {"Alice", "Bob", "Charlie", "Diana", "Eve"};
    std::vector<std::string> actorTemplates = {"AliceTemplate", "BobTemplate", "CharlieTemplate", "DianaTemplate", "EveTemplate"};
    std::vector<std::string> actorDescriptions = {
        "A friendly programmer who loves coffee",
        "An adventurous explorer with a curious mind",
        "A wise philosopher who enjoys deep conversations",
        "A cheerful artist who sees beauty everywhere",
        "A clever engineer who likes solving puzzles"
    };

    int npcCount = config_manager_.getWorldConfig().entities.npc_count;
    for (int i = 0; i < npcCount && i < actorNames.size(); i++) {
        // 尝试使用模板创建Actor
        const EntityTemplate* actorTemplate = getTemplate(actorTemplates[i]);

        entt::entity actor;
        if (actorTemplate) {
            actor = createActorEntity(registry, *actorTemplate);
            std::cout << "Created Actor from template: " << actorNames[i] << std::endl;
        } else {
            // 回退到硬编码创建
            actor = registry.create();
            int x = 3 + (i % 3) * 5;
            int y = 3 + (i / 3) * 4;
            registry.emplace<PositionComponent>(actor, x, y);
            registry.emplace<RenderableComponent>(actor, Color(255, 0, 0, 255), true);
            registry.emplace<ActorComponent>(actor);
            registry.emplace<VelocityComponent>(actor, 0.0f, 0.0f);

            auto& needs = registry.emplace<MaslowNeedsComponent>(actor);
            needs.loveBelonging = config_manager_.getNeedsConfig().social.initial_social_level;
            needs.physiological = config_manager_.getNeedsConfig().physiological.initial_hunger_level;
            needs.fatigue = config_manager_.getNeedsConfig().fatigue.initial_fatigue_level;

            registry.emplace<IdentityComponent>(actor, actorNames[i], "NPC", actorDescriptions[i]);

            auto& knowledge = registry.emplace<KnowledgeComponent>(actor);
            knowledge.addFact(Fact("The sky is blue", entt::null, 0.9f));
            knowledge.addFact(Fact("I am " + actorNames[i], actor, 1.0f));
            knowledge.addFact(Fact("I like talking to people", actor, 0.8f));

            registry.emplace<RelationshipComponent>(actor);
            registry.emplace<OwnershipComponent>(actor);

            std::cout << "Created Actor (fallback): " << actorNames[i] << std::endl;
        }

        // 分配住宅（无论是否使用模板都需要）
        if (registry.all_of<OwnershipComponent>(actor)) {
            auto& ownership = registry.get<OwnershipComponent>(actor);
            auto zones = registry.view<ZoneComponent>();
            for (auto zoneEntity : zones) {
                auto& zone = zones.get<ZoneComponent>(zoneEntity);
                if (zone.name == actorNames[i] + "的家") {
                    ownership.setPrimaryResidence(zoneEntity);
                    zone.owner = actor;
                    std::cout << actorNames[i] << " assigned residence: " << zone.name << std::endl;
                    break;
                }
            }
        }
    }
}

entt::entity WorldFactory::createEntityFromTemplate(entt::registry& registry, const std::string& templateName) {
    const EntityTemplate* tmpl = getTemplate(templateName);
    if (!tmpl) {
        std::cerr << "警告: 未找到模板 '" << templateName << "'" << std::endl;
        return entt::null;
    }

    if (tmpl->type == "Food") {
        return createFoodEntity(registry, *tmpl);
    } else if (tmpl->type == "Zone") {
        return createZoneEntity(registry, *tmpl);
    } else if (tmpl->type == "Actor") {
        return createActorEntity(registry, *tmpl);
    }

    std::cerr << "警告: 未知的实体类型 '" << tmpl->type << "'" << std::endl;
    return entt::null;
}

entt::entity WorldFactory::createFoodEntity(entt::registry& registry, const EntityTemplate& tmpl) {
    auto entity = registry.create();

    // 位置组件
    if (tmpl.position.random) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> xDist(tmpl.position.min_x, tmpl.position.max_x);
        std::uniform_int_distribution<int> yDist(tmpl.position.min_y, tmpl.position.max_y);
        registry.emplace<PositionComponent>(entity, xDist(gen), yDist(gen));
    } else {
        registry.emplace<PositionComponent>(entity, tmpl.position.x, tmpl.position.y);
    }

    // 渲染组件
    if (tmpl.renderable.enabled) {
        Color color(tmpl.renderable.color.r, tmpl.renderable.color.g,
                   tmpl.renderable.color.b, tmpl.renderable.color.a);
        registry.emplace<RenderableComponent>(entity, color, true);
    }

    // 食物组件 - 支持新的食物类型
    std::string foodType = tmpl.components.food.type;
    FoodType actualFoodType = FoodType::BASIC; // 默认值

    if (foodType == "fruit") {
        actualFoodType = FoodType::FRUIT;
    } else if (foodType == "vegetable") {
        actualFoodType = FoodType::VEGETABLE;
    } else if (foodType == "meat") {
        actualFoodType = FoodType::MEAT;
    } else if (foodType == "bread") {
        actualFoodType = FoodType::BREAD;
    } else if (foodType == "dairy") {
        actualFoodType = FoodType::DAIRY;
    }

    registry.emplace<FoodComponent>(entity, actualFoodType, tmpl.components.food.nutrition);

    return entity;
}

entt::entity WorldFactory::createZoneEntity(entt::registry& registry, const EntityTemplate& tmpl) {
    auto entity = registry.create();

    // 位置组件
    registry.emplace<PositionComponent>(entity, tmpl.position.x, tmpl.position.y);

    // 确定区域类型
    ZoneType zoneType = ZoneType::RESIDENCE; // 默认
    if (tmpl.components.zone.zone_type == "tavern") {
        zoneType = ZoneType::TAVERN;
    } else if (tmpl.components.zone.zone_type == "market") {
        zoneType = ZoneType::MARKET;
    }

    // 区域组件
    registry.emplace<ZoneComponent>(entity, zoneType, tmpl.name, tmpl.position.x, tmpl.position.y,
                                   tmpl.components.zone.width, tmpl.components.zone.height);

    return entity;
}

entt::entity WorldFactory::createActorEntity(entt::registry& registry, const EntityTemplate& tmpl) {
    auto entity = registry.create();

    // 位置组件
    registry.emplace<PositionComponent>(entity, tmpl.position.x, tmpl.position.y);

    // 渲染组件
    if (tmpl.renderable.enabled) {
        Color color(tmpl.renderable.color.r, tmpl.renderable.color.g,
                   tmpl.renderable.color.b, tmpl.renderable.color.a);
        registry.emplace<RenderableComponent>(entity, color, true);
    }

    // Actor组件
    registry.emplace<ActorComponent>(entity);
    registry.emplace<VelocityComponent>(entity, 0.0f, 0.0f);

    // 马斯洛需求组件
    auto& needs = registry.emplace<MaslowNeedsComponent>(entity);
    needs.physiological = tmpl.components.actor.initial_hunger;
    needs.fatigue = tmpl.components.actor.initial_fatigue;
    needs.loveBelonging = tmpl.components.actor.initial_social;

    // 身份组件
    registry.emplace<IdentityComponent>(entity, tmpl.components.actor.name,
                                       tmpl.components.actor.role,
                                       tmpl.components.actor.description);

    // 知识组件
    auto& knowledge = registry.emplace<KnowledgeComponent>(entity);
    for (const auto& fact : tmpl.components.actor.initial_facts) {
        knowledge.addFact(Fact(fact, entity, 0.9f));
    }

    // 关系组件
    registry.emplace<RelationshipComponent>(entity);

    // 所有权组件
    registry.emplace<OwnershipComponent>(entity);

    return entity;
}

void WorldFactory::createDefaultWorld(entt::registry& registry) {
    // 如果没有配置文件，使用简单的默认创建
    createFoodEntities(registry);
    createZoneEntities(registry);
    createActorEntities(registry);
}

const EntityTemplate* WorldFactory::getTemplate(const std::string& name) const {
    auto it = std::find_if(templates_.begin(), templates_.end(),
                          [&name](const EntityTemplate& tmpl) {
                              return tmpl.name == name;
                          });
    return (it != templates_.end()) ? &(*it) : nullptr;
}

void WorldFactory::printLoadedTemplates() const {
    std::cout << "已加载的实体模板:" << std::endl;
    for (const auto& tmpl : templates_) {
        std::cout << "  - " << tmpl.name << " (类型: " << tmpl.type << ")" << std::endl;
    }
}

bool WorldFactory::parseEntityTemplates(const std::string& jsonContent) {
    try {
        // 使用nlohmann/json解析整个JSON文档
        json root = json::parse(jsonContent);

        // 检查是否有entity_templates字段
        if (!root.contains("entity_templates")) {
            std::cerr << "JSON文件中未找到 entity_templates 字段" << std::endl;
            return false;
        }

        // 验证entity_templates是数组
        if (!root["entity_templates"].is_array()) {
            std::cerr << "entity_templates 不是数组格式" << std::endl;
            return false;
        }

        // 解析每个模板
        const auto& templatesArray = root["entity_templates"];
        for (const auto& tmplJson : templatesArray) {
            EntityTemplate tmpl = parseEntityTemplateModern(tmplJson);
            if (!tmpl.name.empty()) {
                templates_.push_back(tmpl);
                std::cout << "加载模板: " << tmpl.name << " (类型: " << tmpl.type << ")" << std::endl;
            } else {
                std::cerr << "警告: 跳过无效的实体模板" << std::endl;
            }
        }

        std::cout << "成功解析 " << templates_.size() << " 个实体模板" << std::endl;
        return !templates_.empty();

    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析错误: " << e.what() << " (位置: " << e.byte << ")" << std::endl;
        return false;
    } catch (const json::type_error& e) {
        std::cerr << "JSON类型错误: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "解析实体模板时出错: " << e.what() << std::endl;
        return false;
    }
}

EntityTemplate WorldFactory::parseEntityTemplateModern(const nlohmann::json& tmplJson) {
    EntityTemplate tmpl;

    try {
        // 解析基本信息
        tmpl.name = tmplJson.value("name", "");
        tmpl.type = tmplJson.value("type", "");
        tmpl.description = tmplJson.value("description", "");

        if (tmpl.name.empty() || tmpl.type.empty()) {
            std::cerr << "模板缺少必要字段 name 或 type" << std::endl;
            return tmpl; // 返回空模板表示无效
        }

        // 解析位置信息
        if (tmplJson.contains("position") && tmplJson["position"].is_object()) {
            const auto& posJson = tmplJson["position"];
            tmpl.position.random = posJson.value("random", false);

            if (tmpl.position.random) {
                tmpl.position.min_x = posJson.value("min_x", 0);
                tmpl.position.max_x = posJson.value("max_x", 40);
                tmpl.position.min_y = posJson.value("min_y", 0);
                tmpl.position.max_y = posJson.value("max_y", 40);
            } else {
                tmpl.position.x = posJson.value("x", 0);
                tmpl.position.y = posJson.value("y", 0);
            }
        }

        // 解析渲染信息
        if (tmplJson.contains("renderable") && tmplJson["renderable"].is_object()) {
            const auto& renderJson = tmplJson["renderable"];
            tmpl.renderable.enabled = renderJson.value("enabled", true);

            if (renderJson.contains("color") && renderJson["color"].is_object()) {
                const auto& colorJson = renderJson["color"];
                tmpl.renderable.color.r = colorJson.value("r", 255);
                tmpl.renderable.color.g = colorJson.value("g", 255);
                tmpl.renderable.color.b = colorJson.value("b", 255);
                tmpl.renderable.color.a = colorJson.value("a", 255);
            }
        }

        // 解析组件信息
        if (tmplJson.contains("components") && tmplJson["components"].is_object()) {
            const auto& compJson = tmplJson["components"];

            if (tmpl.type == "Food" && compJson.contains("food") && compJson["food"].is_object()) {
                const auto& foodJson = compJson["food"];
                tmpl.components.food.type = foodJson.value("type", "basic");
                tmpl.components.food.nutrition = foodJson.value("nutrition", 0.5f);

            } else if (tmpl.type == "Zone" && compJson.contains("zone") && compJson["zone"].is_object()) {
                const auto& zoneJson = compJson["zone"];
                tmpl.components.zone.zone_type = zoneJson.value("zone_type", "residence");
                tmpl.components.zone.width = zoneJson.value("width", 4);
                tmpl.components.zone.height = zoneJson.value("height", 4);

            } else if (tmpl.type == "Actor" && compJson.contains("actor") && compJson["actor"].is_object()) {
                const auto& actorJson = compJson["actor"];
                tmpl.components.actor.name = actorJson.value("name", "NPC");
                tmpl.components.actor.role = actorJson.value("role", "NPC");
                tmpl.components.actor.description = actorJson.value("description", "");
                tmpl.components.actor.initial_hunger = actorJson.value("initial_hunger", 0.8f);
                tmpl.components.actor.initial_fatigue = actorJson.value("initial_fatigue", 0.3f);
                tmpl.components.actor.initial_social = actorJson.value("initial_social", 0.8f);

                // 解析初始事实数组
                if (actorJson.contains("initial_facts") && actorJson["initial_facts"].is_array()) {
                    tmpl.components.actor.initial_facts.clear();
                    for (const auto& fact : actorJson["initial_facts"]) {
                        if (fact.is_string()) {
                            tmpl.components.actor.initial_facts.push_back(fact.get<std::string>());
                        }
                    }
                }
            }
        }

    } catch (const json::type_error& e) {
        std::cerr << "解析模板时发生JSON类型错误: " << e.what() << std::endl;
        tmpl.name = ""; // 标记为无效
    } catch (const std::exception& e) {
        std::cerr << "解析模板时出错: " << e.what() << std::endl;
        tmpl.name = ""; // 标记为无效
    }

    return tmpl;
}

EntityTemplate WorldFactory::parseEntityTemplate(const std::string& templateJson) {
    // 旧版本函数，保持向后兼容，现在使用nlohmann/json
    try {
        json tmplJson = json::parse(templateJson);
        return parseEntityTemplateModern(tmplJson);
    } catch (const std::exception& e) {
        std::cerr << "旧版本模板解析失败: " << e.what() << std::endl;
        return EntityTemplate{};
    }
}