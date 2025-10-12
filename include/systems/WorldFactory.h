#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "systems/ConfigManager.h"

// 前向声明nlohmann::json以避免在头文件中包含
// 注释掉前向声明，直接包含以避免命名冲突
// namespace nlohmann {
//     class json;
// }

// 实体模板结构
struct EntityTemplate {
    std::string name;
    std::string type;
    std::string description;

    // 位置配置
    struct Position {
        int x = 0;
        int y = 0;
        bool random = false;
        int min_x = 0;
        int max_x = 0;
        int min_y = 0;
        int max_y = 0;
    } position;

    // 渲染配置
    struct Renderable {
        bool enabled = true;
        struct Color {
            int r = 255;
            int g = 255;
            int b = 255;
            int a = 255;
        } color;
    } renderable;

    // 组件特定配置
    struct Components {
        // 食物组件
        struct Food {
            std::string type = "basic";
            float nutrition = 0.5f;
        } food;

        // 区域组件
        struct Zone {
            std::string zone_type;
            int width = 4;
            int height = 4;
        } zone;

        // Actor组件
        struct Actor {
            std::string name;
            std::string role = "NPC";
            std::string description;

            // 初始需求值
            float initial_hunger = 0.8f;
            float initial_fatigue = 0.3f;
            float initial_social = 0.8f;

            // 初始知识
            std::vector<std::string> initial_facts;
        } actor;
    } components;
};

// 世界工厂类，负责从配置文件创建实体
class WorldFactory {
public:
    // 构造函数 - 接收 ConfigManager 依赖
    explicit WorldFactory(const ConfigManager& configManager);

    // 主要接口方法
    bool loadEntityTemplates(const std::string& templatesPath = "config/entities.json");

    // 实体创建方法
    void createWorld(entt::registry& registry);
    void createEntitiesByType(entt::registry& registry, const std::string& entityType);

    // 析构函数
    ~WorldFactory() = default;

private:
    // 内部实体创建方法
    void createDefaultWorld(entt::registry& registry);
    void createFoodEntities(entt::registry& registry);
    void createZoneEntities(entt::registry& registry);
    void createActorEntities(entt::registry& registry);

    // 单个实体创建
    entt::entity createEntityFromTemplate(entt::registry& registry, const std::string& templateName);

    // 获取已加载的模板
    const std::vector<EntityTemplate>& getTemplates() const { return templates_; }
    const EntityTemplate* getTemplate(const std::string& name) const;

    // 调试方法
    void printLoadedTemplates() const;

private:
    WorldFactory(const WorldFactory&) = delete;
    WorldFactory& operator=(const WorldFactory&) = delete;

    // 实体创建辅助方法
    entt::entity createFoodEntity(entt::registry& registry, const EntityTemplate& tmpl);
    entt::entity createZoneEntity(entt::registry& registry, const EntityTemplate& tmpl);
    entt::entity createActorEntity(entt::registry& registry, const EntityTemplate& tmpl);

    // JSON解析方法
    bool parseEntityTemplates(const std::string& jsonContent);
    EntityTemplate parseEntityTemplate(const std::string& templateJson);  // 旧版本，保持向后兼容
    EntityTemplate parseEntityTemplateModern(const nlohmann::json& tmplJson);  // 新版本，使用nlohmann/json

    // 成员变量
    const ConfigManager& config_manager_;  // 配置管理器引用
    std::vector<EntityTemplate> templates_;
    std::string templates_path_;
    bool templates_loaded_ = false;
};