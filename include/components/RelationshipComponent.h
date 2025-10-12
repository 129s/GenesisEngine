#pragma once

#include <map>
#include <entt/entt.hpp>
#include <string>

// 人际关系组件
struct RelationshipComponent {
    // Key: 另一个实体的ID, Value: 好感度 (-100 to 100)
    std::map<entt::entity, float> relations;

    RelationshipComponent() = default;

    // 设置与另一个实体的关系
    void setRelationship(entt::entity target, float relationshipValue);

    // 获取与另一个实体的关系
    float getRelationship(entt::entity target) const;

    // 改善关系
    void improveRelationship(entt::entity target, float amount = 5.0f);

    // 恶化关系
    void deteriorateRelationship(entt::entity target, float amount = 5.0f);

    // 获取最佳朋友（好感度最高的实体）
    entt::entity getBestFriend() const;

    // 获取关系描述
    std::string getRelationshipDescription(entt::entity target) const;

    // 清空所有关系
    void clear();

    // 获取关系数量
    size_t size() const;
};