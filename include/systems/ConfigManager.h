#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 显示配置结构
struct DisplayConfig {
    int window_width = 1120;
    int window_height = 720;
    std::string window_title = "Genesis Engine - Debug Mode";
    int game_width = 720;
    int game_height = 720;
    int tile_size = 16;
    int debug_ui_width = 400;
    bool debug_ui_enabled = true;
};

// from_json 和 to_json 支持
void from_json(const json& j, DisplayConfig& config);
void to_json(json& j, const DisplayConfig& config);

// AI配置结构
struct AIConfig {
    struct WanderConfig {
        float trigger_interval_seconds = 2.5f;
        float movement_speed = 1.0f;
        int direction_choices = 5;
        float stop_probability = 0.2f;
    } wander;

    struct MovementConfig {
        float move_interval_seconds = 0.5f;
        int tiles_per_move = 1;
    } movement;
};

// from_json 和 to_json 支持
void from_json(const json& j, AIConfig& config);
void to_json(json& j, const AIConfig& config);
void from_json(const json& j, AIConfig::WanderConfig& config);
void to_json(json& j, const AIConfig::WanderConfig& config);
void from_json(const json& j, AIConfig::MovementConfig& config);
void to_json(json& j, const AIConfig::MovementConfig& config);

// 需求系统配置结构
struct NeedsConfig {
    struct PhysiologicalConfig {
        float hunger_increase_rate = 0.02f;
        float initial_hunger_level = 0.8f;
        float hunger_threshold = 0.7f;
        float critical_hunger_threshold = 0.9f;
    } physiological;

    struct FatigueConfig {
        float activity_increase_rate = 0.03f;
        float night_activity_bonus = 0.02f;
        float rest_recovery_rate = 0.05f;
        float initial_fatigue_level = 0.3f;
        float fatigue_threshold = 0.7f;
        float critical_fatigue_threshold = 0.9f;
    } fatigue;

    struct SocialConfig {
        float increase_rate = 0.01f;
        float initial_social_level = 0.8f;
        float social_threshold = 0.6f;
    } social;
};

// from_json 和 to_json 支持
void from_json(const json& j, NeedsConfig& config);
void to_json(json& j, const NeedsConfig& config);
void from_json(const json& j, NeedsConfig::PhysiologicalConfig& config);
void to_json(json& j, const NeedsConfig::PhysiologicalConfig& config);
void from_json(const json& j, NeedsConfig::FatigueConfig& config);
void to_json(json& j, const NeedsConfig::FatigueConfig& config);
void from_json(const json& j, NeedsConfig::SocialConfig& config);
void to_json(json& j, const NeedsConfig::SocialConfig& config);

// 食物配置结构
struct FoodConfig {
    float default_nutrition_value = 0.5f;
    float min_nutrition_value = 0.3f;
    float max_nutrition_value = 0.7f;
};

// 世界配置结构
struct WorldConfig {
    struct EntitiesConfig {
        int food_count = 8;
        int npc_count = 5;
        int residence_count = 5;
    } entities;

    struct ZonesConfig {
        struct ZoneSize {
            int width = 4;
            int height = 4;
        };
        ZoneSize residence{4, 4};
        ZoneSize tavern{8, 6};
        ZoneSize market{6, 5};
    } zones;
};

// 时间配置结构
struct TimeConfig {
    int ticks_per_update = 1;
    int hours_per_tick = 1;
};

// 调试配置结构
struct DebugConfig {
    bool enable_console_output = true;
    bool enable_debug_ui = true;
};

// from_json 和 to_json 支持
void from_json(const json& j, FoodConfig& config);
void to_json(json& j, const FoodConfig& config);
void from_json(const json& j, WorldConfig& config);
void to_json(json& j, const WorldConfig& config);
void from_json(const json& j, WorldConfig::EntitiesConfig& config);
void to_json(json& j, const WorldConfig::EntitiesConfig& config);
void from_json(const json& j, WorldConfig::ZonesConfig& config);
void to_json(json& j, const WorldConfig::ZonesConfig& config);
void from_json(const json& j, WorldConfig::ZonesConfig::ZoneSize& config);
void to_json(json& j, const WorldConfig::ZonesConfig::ZoneSize& config);
void from_json(const json& j, TimeConfig& config);
void to_json(json& j, const TimeConfig& config);
void from_json(const json& j, DebugConfig& config);
void to_json(json& j, const DebugConfig& config);

// 主配置管理器
class ConfigManager {
public:
    // 构造函数 - 支持依赖注入
    ConfigManager() = default;

    // 删除单例模式相关函数
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 加载配置文件
    bool loadConfig(const std::string& configPath = "config/game_config.json");

    // 获取各种配置
    const DisplayConfig& getDisplayConfig() const { return display_config_; }
    const AIConfig& getAIConfig() const { return ai_config_; }
    const NeedsConfig& getNeedsConfig() const { return needs_config_; }
    const FoodConfig& getFoodConfig() const { return food_config_; }
    const WorldConfig& getWorldConfig() const { return world_config_; }
    const TimeConfig& getTimeConfig() const { return time_config_; }
    const DebugConfig& getDebugConfig() const { return debug_config_; }

    // 便捷访问方法
    int getWindowWidth() const { return display_config_.window_width; }
    int getWindowHeight() const { return display_config_.window_height; }
    int getGameWidth() const { return display_config_.game_width; }
    int getGameHeight() const { return display_config_.game_height; }
    int getTileSize() const { return display_config_.tile_size; }

    // 重新加载配置
    bool reloadConfig();

    // 设置配置值（用于运行时修改）
    void setWindowWidth(int width) { display_config_.window_width = width; }
    void setWindowHeight(int height) { display_config_.window_height = height; }

    // 验证配置值的有效性
    bool validateConfig() const;

    // 输出当前配置（用于调试）
    void printConfig() const;

private:
    // 解析JSON配置的私有方法 - 使用 nlohmann/json
    void parseConfigFromJson(const json& rootJson);

    // 配置成员变量
    DisplayConfig display_config_;
    AIConfig ai_config_;
    NeedsConfig needs_config_;
    FoodConfig food_config_;
    WorldConfig world_config_;
    TimeConfig time_config_;
    DebugConfig debug_config_;

    std::string config_path_;
    bool config_loaded_ = false;
};

// 注意：不再使用全局访问宏，推荐通过依赖注入使用 ConfigManager