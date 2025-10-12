#include "components/RelationshipComponent.h"

void RelationshipComponent::setRelationship(entt::entity target, float relationshipValue) {
    relations[target] = relationshipValue;
}

float RelationshipComponent::getRelationship(entt::entity target) const {
    auto it = relations.find(target);
    if (it != relations.end()) {
        return it->second;
    }
    return 0.0f; // 默认中性关系
}

void RelationshipComponent::improveRelationship(entt::entity target, float amount) {
    relations[target] += amount;
    // 限制在最大值
    if (relations[target] > 100.0f) {
        relations[target] = 100.0f;
    }
}

void RelationshipComponent::deteriorateRelationship(entt::entity target, float amount) {
    relations[target] -= amount;
    // 限制在最小值
    if (relations[target] < -100.0f) {
        relations[target] = -100.0f;
    }
}

entt::entity RelationshipComponent::getBestFriend() const {
    if (relations.empty()) {
        return entt::null;
    }

    entt::entity bestFriend = entt::null;
    float highestRelationship = -100.0f;

    for (const auto& pair : relations) {
        if (pair.second > highestRelationship) {
            highestRelationship = pair.second;
            bestFriend = pair.first;
        }
    }

    return bestFriend;
}

std::string RelationshipComponent::getRelationshipDescription(entt::entity target) const {
    float relationship = getRelationship(target);
    if (relationship > 70.0f) return "最好的朋友";
    else if (relationship > 30.0f) return "朋友";
    else if (relationship > -30.0f) return "认识的人";
    else if (relationship > -70.0f) return "不喜欢的人";
    else return "讨厌的人";
}

void RelationshipComponent::clear() {
    relations.clear();
}

size_t RelationshipComponent::size() const {
    return relations.size();
}