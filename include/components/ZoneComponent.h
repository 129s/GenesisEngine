#pragma once

#include <string>
#include <entt/entt.hpp>

// 区域类型枚举
enum class ZoneType {
    RESIDENCE,     // 住宅区
    TAVERN,        // 酒馆/社交区
    MARKET,        // 市场
    WORKPLACE,     // 工作场所
    PUBLIC         // 公共区域
};

// 区域组件 - 定义一个功能区域
struct ZoneComponent {
    ZoneType type;
    std::string name;           // 区域名称，如"Alice的家"、"金色酒馆"
    entt::entity owner = entt::null;  // 区域所有者（如果是住宅）

    // 区域边界（矩形区域）
    int x, y;
    int width, height;

    // 区域属性
    bool isPrivate = false;     // 是否为私有区域
    bool isSocialZone = false;  // 是否为社交区域

    ZoneComponent(ZoneType t, const std::string& n, int posX, int posY, int w, int h);

    // 检查点是否在区域内
    bool contains(int pointX, int pointY) const;

    // 获取区域中心点
    std::pair<int, int> getCenter() const;

    // 获取区域类型名称
    std::string getTypeName() const;
};