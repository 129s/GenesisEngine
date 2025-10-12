#include "components/OwnershipComponent.h"
#include <algorithm>

void OwnershipComponent::addOwnedEntity(entt::entity entity) {
    ownedEntities.push_back(entity);
}

void OwnershipComponent::setPrimaryResidence(entt::entity residence) {
    primaryResidence = residence;
    // 确保住所也在拥有列表中
    auto it = std::find(ownedEntities.begin(), ownedEntities.end(), residence);
    if (it == ownedEntities.end()) {
        ownedEntities.push_back(residence);
    }
}

void OwnershipComponent::removeOwnedEntity(entt::entity entity) {
    ownedEntities.erase(
        std::remove(ownedEntities.begin(), ownedEntities.end(), entity),
        ownedEntities.end()
    );
    // 如果移除的是主要住所，清空
    if (primaryResidence == entity) {
        primaryResidence = entt::null;
    }
}

bool OwnershipComponent::ownsEntity(entt::entity entity) const {
    return std::find(ownedEntities.begin(), ownedEntities.end(), entity) != ownedEntities.end();
}

bool OwnershipComponent::hasResidence() const {
    return primaryResidence != entt::null;
}

size_t OwnershipComponent::getOwnedCount() const {
    return ownedEntities.size();
}