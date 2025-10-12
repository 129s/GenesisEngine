#pragma once

#include <string>
#include <entt/entt.hpp>

// 身份组件，方便调试
struct IdentityComponent {
    std::string name;
    std::string role;
    std::string description;

    IdentityComponent() = default;
    IdentityComponent(const std::string& entityName, const std::string& entityRole = "", const std::string& entityDescription = "")
        : name(entityName), role(entityRole), description(entityDescription) {}

    // 获取完整身份信息
    std::string getFullName() const;

    // 获取描述信息
    std::string getFullDescription() const;
};