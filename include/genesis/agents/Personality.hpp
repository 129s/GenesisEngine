#pragma once

namespace genesis::agents {

struct AgentPersonalityBig5 {
    // Big Five dimensions in range [0,1]
    float openness{0.5f};
    float conscientiousness{0.5f};
    float extraversion{0.5f};
    float agreeableness{0.5f};
    float neuroticism{0.5f};
};

} // namespace genesis::agents

