#include "components/ActionQueueComponent.h"

void ActionQueueComponent::addAction(const Action& action) {
    actions.push_back(action);
}

void ActionQueueComponent::addPriorityAction(const Action& action) {
    actions.insert(actions.begin(), action);
}

Action ActionQueueComponent::getNextAction() const {
    if (!actions.empty()) {
        return actions.front();
    }
    return Action(ActionType::MOVE_TO, entt::null);
}

void ActionQueueComponent::completeCurrentAction() {
    if (!actions.empty()) {
        actions.erase(actions.begin());
    }
}

bool ActionQueueComponent::isEmpty() const {
    return actions.empty();
}

void ActionQueueComponent::clear() {
    actions.clear();
}