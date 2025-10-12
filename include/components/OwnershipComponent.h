#pragma once

#include <entt/entt.hpp>
#include <vector>

// 所有权组件 - 定义实体对其他实体（如住宅）的所有权
struct OwnershipComponent {
    // 拥有的实体列表（如住宅、商店等）
    std::vector<entt::entity> ownedEntities;

    // 主要住所（如果有的话）
    entt::entity primaryResidence = entt::null;

    // 添加拥有的实体
    void addOwnedEntity(entt::entity entity);

    // 设置主要住所
    void setPrimaryResidence(entt::entity residence);

    // 移除拥有的实体
    void removeOwnedEntity(entt::entity entity);

    // 检查是否拥有某个实体
    bool ownsEntity(entt::entity entity) const;

    // 是否有住所
    bool hasResidence() const;

    // 获取拥有的实体数量
    size_t getOwnedCount() const;
};