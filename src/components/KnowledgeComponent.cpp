#include "components/KnowledgeComponent.h"
#include <cstdlib>

void KnowledgeComponent::addFact(const Fact& fact) {
    knownFacts.push_back(fact);
}

const Fact* KnowledgeComponent::getRandomFact() const {
    if (knownFacts.empty()) {
        return nullptr;
    }

    size_t index = static_cast<size_t>(rand()) % knownFacts.size();
    return &knownFacts[index];
}

void KnowledgeComponent::clear() {
    knownFacts.clear();
}

size_t KnowledgeComponent::size() const {
    return knownFacts.size();
}