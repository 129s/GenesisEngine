#include "components/IdentityComponent.h"

std::string IdentityComponent::getFullName() const {
    if (role.empty()) {
        return name;
    }
    return name + " (" + role + ")";
}

std::string IdentityComponent::getFullDescription() const {
    std::string fullDesc = getFullName();
    if (!description.empty()) {
        fullDesc += ": " + description;
    }
    return fullDesc;
}