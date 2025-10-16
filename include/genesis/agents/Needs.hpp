#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace genesis::agents {

enum class NeedType : std::uint8_t {
    Hunger = 0,
    Energy = 1,
    Social = 2,
    Count
};

constexpr std::size_t needTypeCount() {
    return static_cast<std::size_t>(NeedType::Count);
}

constexpr std::size_t needIndex(NeedType type) {
    return static_cast<std::size_t>(type);
}

struct NeedDescriptor {
    NeedType type{NeedType::Hunger};
    float minValue{0.0f};
    float maxValue{100.0f};
    float decayPerSecond{0.0f};
    float satisfiedThreshold{30.0f};
    float criticalThreshold{80.0f};
};

struct NeedState {
    NeedType type{NeedType::Hunger};
    float value{0.0f};
    float modifier{1.0f};

    void clamp(const NeedDescriptor& descriptor) {
        if (value < descriptor.minValue) {
            value = descriptor.minValue;
        } else if (value > descriptor.maxValue) {
            value = descriptor.maxValue;
        }
    }
};

struct NeedSample {
    NeedType type{NeedType::Hunger};
    float value{0.0f};
    bool satisfied{false};
    bool critical{false};
};

class NeedRegulator {
public:
    NeedSample update(float deltaSeconds, NeedState& state, const NeedDescriptor& descriptor) const {
        if (deltaSeconds <= 0.0f) {
            return summarise(state, descriptor);
        }

        const float decay = descriptor.decayPerSecond * deltaSeconds * state.modifier;
        state.value += decay;
        state.clamp(descriptor);
        return summarise(state, descriptor);
    }

    [[nodiscard]] NeedSample summarise(const NeedState& state, const NeedDescriptor& descriptor) const {
        NeedSample sample{};
        sample.type = state.type;
        sample.value = state.value;
        sample.satisfied = state.value <= descriptor.satisfiedThreshold;
        sample.critical = state.value >= descriptor.criticalThreshold;
        return sample;
    }
};

class NeedCollection {
public:
    void setDescriptor(const NeedDescriptor& descriptor) {
        const auto idx = needIndex(descriptor.type);
        m_descriptors[idx] = descriptor;
        if (!m_states[idx].has_value()) {
            NeedState state{};
            state.type = descriptor.type;
            state.value = descriptor.minValue;
            m_states[idx] = state;
        }
    }

    void setState(NeedType type, float value, float modifier = 1.0f) {
        const auto idx = needIndex(type);
        if (!m_states[idx].has_value()) {
            NeedState state{};
            state.type = type;
            state.value = value;
            state.modifier = modifier;
            m_states[idx] = state;
        } else {
            auto& state = *m_states[idx];
            state.value = value;
            state.modifier = modifier;
        }
        if (m_descriptors[idx].has_value()) {
            m_states[idx]->clamp(*m_descriptors[idx]);
        }
    }

    [[nodiscard]] NeedState* state(NeedType type) {
        const auto idx = needIndex(type);
        if (m_states[idx].has_value()) {
            return &*m_states[idx];
        }
        return nullptr;
    }

    [[nodiscard]] const NeedState* state(NeedType type) const {
        const auto idx = needIndex(type);
        if (m_states[idx].has_value()) {
            return &*m_states[idx];
        }
        return nullptr;
    }

    [[nodiscard]] const NeedDescriptor* descriptor(NeedType type) const {
        const auto idx = needIndex(type);
        if (m_descriptors[idx].has_value()) {
            return &*m_descriptors[idx];
        }
        return nullptr;
    }

    template <typename Fn>
    void forEach(Fn&& fn) {
        for (std::size_t idx = 0; idx < needTypeCount(); ++idx) {
            if (m_states[idx].has_value() && m_descriptors[idx].has_value()) {
                fn(*m_states[idx], *m_descriptors[idx]);
            }
        }
    }

    template <typename Fn>
    void forEach(Fn&& fn) const {
        for (std::size_t idx = 0; idx < needTypeCount(); ++idx) {
            if (m_states[idx].has_value() && m_descriptors[idx].has_value()) {
                fn(*m_states[idx], *m_descriptors[idx]);
            }
        }
    }

private:
    std::array<std::optional<NeedDescriptor>, needTypeCount()> m_descriptors{};
    std::array<std::optional<NeedState>, needTypeCount()> m_states{};
};

} // namespace genesis::agents
