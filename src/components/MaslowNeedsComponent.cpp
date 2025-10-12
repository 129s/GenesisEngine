#include "components/MaslowNeedsComponent.h"

std::string MaslowNeedsComponent::getMostUrgentNeed() const {
    if (physiological >= CRITICAL_HUNGER_THRESHOLD) {
        return "EXTREMELY_HUNGRY";
    } else if (fatigue >= CRITICAL_FATIGUE_THRESHOLD) {
        return "EXHAUSTED";
    } else if (physiological >= HUNGER_THRESHOLD) {
        return "HUNGRY";
    } else if (fatigue >= FATIGUE_THRESHOLD) {
        return "TIRED";
    } else if (loveBelonging >= SOCIAL_THRESHOLD) {
        return "LONELY";
    }
    return "SATISFIED";
}

bool MaslowNeedsComponent::needsFood() const {
    return physiological >= HUNGER_THRESHOLD;
}

bool MaslowNeedsComponent::needsRest() const {
    return fatigue >= FATIGUE_THRESHOLD;
}

bool MaslowNeedsComponent::needsSocialization() const {
    return loveBelonging >= SOCIAL_THRESHOLD;
}