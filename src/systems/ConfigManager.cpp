#include "systems/ConfigManager.h"
#include <iostream>
#include <fstream>
#include <algorithm>

// === from_json 和 to_json 函数实现 ===

// DisplayConfig
void from_json(const json& j, DisplayConfig& config) {
    if (j.contains("display")) {
        const auto& display = j["display"];
        if (display.contains("width")) config.window_width = display["width"];
        if (display.contains("height")) config.window_height = display["height"];
        if (display.contains("title")) config.window_title = display["title"];
    }

    if (j.contains("game_area")) {
        const auto& game_area = j["game_area"];
        if (game_area.contains("width")) config.game_width = game_area["width"];
        if (game_area.contains("height")) config.game_height = game_area["height"];
        if (game_area.contains("tile_size")) config.tile_size = game_area["tile_size"];
    }

    if (j.contains("debug_ui")) {
        const auto& debug_ui = j["debug_ui"];
        if (debug_ui.contains("width")) config.debug_ui_width = debug_ui["width"];
        if (debug_ui.contains("enabled")) config.debug_ui_enabled = debug_ui["enabled"];
    }
}

void to_json(json& j, const DisplayConfig& config) {
    j["display"]["width"] = config.window_width;
    j["display"]["height"] = config.window_height;
    j["display"]["title"] = config.window_title;
    j["game_area"]["width"] = config.game_width;
    j["game_area"]["height"] = config.game_height;
    j["game_area"]["tile_size"] = config.tile_size;
    j["debug_ui"]["width"] = config.debug_ui_width;
    j["debug_ui"]["enabled"] = config.debug_ui_enabled;
}

// AIConfig::WanderConfig
void from_json(const json& j, AIConfig::WanderConfig& config) {
    if (j.contains("trigger_interval_seconds")) config.trigger_interval_seconds = j["trigger_interval_seconds"];
    if (j.contains("movement_speed")) config.movement_speed = j["movement_speed"];
    if (j.contains("direction_choices")) config.direction_choices = j["direction_choices"];
    if (j.contains("stop_probability")) config.stop_probability = j["stop_probability"];
}

void to_json(json& j, const AIConfig::WanderConfig& config) {
    j["trigger_interval_seconds"] = config.trigger_interval_seconds;
    j["movement_speed"] = config.movement_speed;
    j["direction_choices"] = config.direction_choices;
    j["stop_probability"] = config.stop_probability;
}

// AIConfig::MovementConfig
void from_json(const json& j, AIConfig::MovementConfig& config) {
    if (j.contains("move_interval_seconds")) config.move_interval_seconds = j["move_interval_seconds"];
    if (j.contains("tiles_per_move")) config.tiles_per_move = j["tiles_per_move"];
}

void to_json(json& j, const AIConfig::MovementConfig& config) {
    j["move_interval_seconds"] = config.move_interval_seconds;
    j["tiles_per_move"] = config.tiles_per_move;
}

// AIConfig
void from_json(const json& j, AIConfig& config) {
    if (j.contains("wander")) config.wander = j["wander"];
    if (j.contains("movement")) config.movement = j["movement"];
}

void to_json(json& j, const AIConfig& config) {
    j["wander"] = config.wander;
    j["movement"] = config.movement;
}

// NeedsConfig::PhysiologicalConfig
void from_json(const json& j, NeedsConfig::PhysiologicalConfig& config) {
    if (j.contains("hunger_increase_rate")) config.hunger_increase_rate = j["hunger_increase_rate"];
    if (j.contains("initial_hunger_level")) config.initial_hunger_level = j["initial_hunger_level"];
    if (j.contains("hunger_threshold")) config.hunger_threshold = j["hunger_threshold"];
    if (j.contains("critical_hunger_threshold")) config.critical_hunger_threshold = j["critical_hunger_threshold"];
}

void to_json(json& j, const NeedsConfig::PhysiologicalConfig& config) {
    j["hunger_increase_rate"] = config.hunger_increase_rate;
    j["initial_hunger_level"] = config.initial_hunger_level;
    j["hunger_threshold"] = config.hunger_threshold;
    j["critical_hunger_threshold"] = config.critical_hunger_threshold;
}

// NeedsConfig::FatigueConfig
void from_json(const json& j, NeedsConfig::FatigueConfig& config) {
    if (j.contains("activity_increase_rate")) config.activity_increase_rate = j["activity_increase_rate"];
    if (j.contains("night_activity_bonus")) config.night_activity_bonus = j["night_activity_bonus"];
    if (j.contains("rest_recovery_rate")) config.rest_recovery_rate = j["rest_recovery_rate"];
    if (j.contains("initial_fatigue_level")) config.initial_fatigue_level = j["initial_fatigue_level"];
    if (j.contains("fatigue_threshold")) config.fatigue_threshold = j["fatigue_threshold"];
    if (j.contains("critical_fatigue_threshold")) config.critical_fatigue_threshold = j["critical_fatigue_threshold"];
}

void to_json(json& j, const NeedsConfig::FatigueConfig& config) {
    j["activity_increase_rate"] = config.activity_increase_rate;
    j["night_activity_bonus"] = config.night_activity_bonus;
    j["rest_recovery_rate"] = config.rest_recovery_rate;
    j["initial_fatigue_level"] = config.initial_fatigue_level;
    j["fatigue_threshold"] = config.fatigue_threshold;
    j["critical_fatigue_threshold"] = config.critical_fatigue_threshold;
}

// NeedsConfig::SocialConfig
void from_json(const json& j, NeedsConfig::SocialConfig& config) {
    if (j.contains("increase_rate")) config.increase_rate = j["increase_rate"];
    if (j.contains("initial_social_level")) config.initial_social_level = j["initial_social_level"];
    if (j.contains("social_threshold")) config.social_threshold = j["social_threshold"];
}

void to_json(json& j, const NeedsConfig::SocialConfig& config) {
    j["increase_rate"] = config.increase_rate;
    j["initial_social_level"] = config.initial_social_level;
    j["social_threshold"] = config.social_threshold;
}

// NeedsConfig
void from_json(const json& j, NeedsConfig& config) {
    if (j.contains("physiological")) config.physiological = j["physiological"];
    if (j.contains("fatigue")) config.fatigue = j["fatigue"];
    if (j.contains("social")) config.social = j["social"];
}

void to_json(json& j, const NeedsConfig& config) {
    j["physiological"] = config.physiological;
    j["fatigue"] = config.fatigue;
    j["social"] = config.social;
}

// FoodConfig
void from_json(const json& j, FoodConfig& config) {
    if (j.contains("default_nutrition_value")) config.default_nutrition_value = j["default_nutrition_value"];
    if (j.contains("min_nutrition_value")) config.min_nutrition_value = j["min_nutrition_value"];
    if (j.contains("max_nutrition_value")) config.max_nutrition_value = j["max_nutrition_value"];
}

void to_json(json& j, const FoodConfig& config) {
    j["default_nutrition_value"] = config.default_nutrition_value;
    j["min_nutrition_value"] = config.min_nutrition_value;
    j["max_nutrition_value"] = config.max_nutrition_value;
}

// WorldConfig::ZonesConfig::ZoneSize
void from_json(const json& j, WorldConfig::ZonesConfig::ZoneSize& config) {
    if (j.contains("width")) config.width = j["width"];
    if (j.contains("height")) config.height = j["height"];
}

void to_json(json& j, const WorldConfig::ZonesConfig::ZoneSize& config) {
    j["width"] = config.width;
    j["height"] = config.height;
}

// WorldConfig::ZonesConfig
void from_json(const json& j, WorldConfig::ZonesConfig& config) {
    if (j.contains("residence")) config.residence = j["residence"];
    if (j.contains("tavern")) config.tavern = j["tavern"];
    if (j.contains("market")) config.market = j["market"];
}

void to_json(json& j, const WorldConfig::ZonesConfig& config) {
    j["residence"] = config.residence;
    j["tavern"] = config.tavern;
    j["market"] = config.market;
}

// WorldConfig::EntitiesConfig
void from_json(const json& j, WorldConfig::EntitiesConfig& config) {
    if (j.contains("food_count")) config.food_count = j["food_count"];
    if (j.contains("npc_count")) config.npc_count = j["npc_count"];
    if (j.contains("residence_count")) config.residence_count = j["residence_count"];
}

void to_json(json& j, const WorldConfig::EntitiesConfig& config) {
    j["food_count"] = config.food_count;
    j["npc_count"] = config.npc_count;
    j["residence_count"] = config.residence_count;
}

// WorldConfig
void from_json(const json& j, WorldConfig& config) {
    if (j.contains("entities")) config.entities = j["entities"];
    if (j.contains("zones")) config.zones = j["zones"];
}

void to_json(json& j, const WorldConfig& config) {
    j["entities"] = config.entities;
    j["zones"] = config.zones;
}

// TimeConfig
void from_json(const json& j, TimeConfig& config) {
    if (j.contains("ticks_per_update")) config.ticks_per_update = j["ticks_per_update"];
    if (j.contains("hours_per_tick")) config.hours_per_tick = j["hours_per_tick"];
}

void to_json(json& j, const TimeConfig& config) {
    j["ticks_per_update"] = config.ticks_per_update;
    j["hours_per_tick"] = config.hours_per_tick;
}

// DebugConfig
void from_json(const json& j, DebugConfig& config) {
    if (j.contains("enable_console_output")) config.enable_console_output = j["enable_console_output"];
    if (j.contains("enable_debug_ui")) config.enable_debug_ui = j["enable_debug_ui"];
}

void to_json(json& j, const DebugConfig& config) {
    j["enable_console_output"] = config.enable_console_output;
    j["enable_debug_ui"] = config.enable_debug_ui;
}

// === ConfigManager 实现 ===

bool ConfigManager::loadConfig(const std::string& configPath) {
    config_path_ = configPath;

    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "警告: 无法打开配置文件 " << configPath
                  << "，将使用默认配置" << std::endl;
        return false;
    }

    try {
        json rootJson;
        file >> rootJson;
        file.close();

        // 使用 nlohmann/json 自动反序列化
        parseConfigFromJson(rootJson);

        config_loaded_ = true;

        if (debug_config_.enable_console_output) {
            std::cout << "配置文件加载成功: " << configPath << std::endl;
        }

        return validateConfig();
    }
    catch (const std::exception& e) {
        std::cerr << "配置文件解析失败: " << e.what()
                  << "，将使用默认配置" << std::endl;
        return false;
    }
}

bool ConfigManager::reloadConfig() {
    config_loaded_ = false;
    return loadConfig(config_path_);
}

bool ConfigManager::validateConfig() const {
    bool valid = true;

    // 验证显示配置
    if (display_config_.window_width <= 0 || display_config_.window_height <= 0) {
        std::cerr << "错误: 窗口尺寸无效" << std::endl;
        valid = false;
    }

    if (display_config_.game_width <= 0 || display_config_.game_height <= 0) {
        std::cerr << "错误: 游戏区域尺寸无效" << std::endl;
        valid = false;
    }

    if (display_config_.tile_size <= 0) {
        std::cerr << "错误: 瓦片尺寸无效" << std::endl;
        valid = false;
    }

    // 验证AI配置
    if (ai_config_.wander.trigger_interval_seconds <= 0) {
        std::cerr << "错误: 游荡AI触发间隔无效" << std::endl;
        valid = false;
    }

    if (ai_config_.movement.move_interval_seconds <= 0) {
        std::cerr << "错误: 移动间隔无效" << std::endl;
        valid = false;
    }

    // 验证需求配置
    if (needs_config_.physiological.hunger_increase_rate < 0) {
        std::cerr << "错误: 饥饿增长率不能为负数" << std::endl;
        valid = false;
    }

    // 验证世界配置
    if (world_config_.entities.food_count < 0 || world_config_.entities.npc_count < 0) {
        std::cerr << "错误: 实体数量不能为负数" << std::endl;
        valid = false;
    }

    return valid;
}

void ConfigManager::printConfig() const {
    std::cout << "\n=== 当前游戏配置 ===" << std::endl;
    std::cout << "窗口尺寸: " << display_config_.window_width
              << "x" << display_config_.window_height << std::endl;
    std::cout << "游戏区域: " << display_config_.game_width
              << "x" << display_config_.game_height << std::endl;
    std::cout << "瓦片尺寸: " << display_config_.tile_size << std::endl;
    std::cout << "游荡AI间隔: " << ai_config_.wander.trigger_interval_seconds << "秒" << std::endl;
    std::cout << "移动间隔: " << ai_config_.movement.move_interval_seconds << "秒" << std::endl;
    std::cout << "饥饿增长率: " << needs_config_.physiological.hunger_increase_rate << std::endl;
    std::cout << "疲劳增长率: " << needs_config_.fatigue.activity_increase_rate << std::endl;
    std::cout << "食物数量: " << world_config_.entities.food_count << std::endl;
    std::cout << "NPC数量: " << world_config_.entities.npc_count << std::endl;
    std::cout << "==================\n" << std::endl;
}

void ConfigManager::parseConfigFromJson(const json& rootJson) {
    // 使用 nlohmann/json 自动反序列化各个配置部分
    if (rootJson.contains("display") || rootJson.contains("game_area") || rootJson.contains("debug_ui")) {
        display_config_ = rootJson;
    }

    if (rootJson.contains("ai")) {
        ai_config_ = rootJson["ai"];
    }

    if (rootJson.contains("needs")) {
        needs_config_ = rootJson["needs"];
    }

    if (rootJson.contains("food")) {
        food_config_ = rootJson["food"];
    }

    if (rootJson.contains("world")) {
        world_config_ = rootJson["world"];
    }

    if (rootJson.contains("time")) {
        time_config_ = rootJson["time"];
    }

    // 注意：debug 在 JSON 中可能叫 "debugging"
    if (rootJson.contains("debugging")) {
        debug_config_ = rootJson["debugging"];
    } else if (rootJson.contains("debug")) {
        debug_config_ = rootJson["debug"];
    }
}