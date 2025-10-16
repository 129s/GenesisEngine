#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"

using genesis::agents::NeedComponent;
using genesis::agents::NeedDescriptor;
using genesis::agents::NeedRegulator;
using genesis::agents::NeedState;
using genesis::agents::NeedType;
using genesis::agents::needIndex;

TEST(NeedRegulatorTest, AccumulatesAndClamps) {
    NeedDescriptor descriptor{};
    descriptor.type = NeedType::Hunger;
    descriptor.minValue = 0.0f;
    descriptor.maxValue = 100.0f;
    descriptor.decayPerSecond = 5.0f;
    descriptor.satisfiedThreshold = 20.0f;
    descriptor.criticalThreshold = 80.0f;

    NeedState state{};
    state.type = NeedType::Hunger;
    state.value = 0.0f;

    NeedRegulator regulator;

    auto sample = regulator.update(1.0f, state, descriptor);
    EXPECT_FLOAT_EQ(state.value, 5.0f);
    EXPECT_FALSE(sample.critical);

    sample = regulator.update(20.0f, state, descriptor);
    EXPECT_FLOAT_EQ(state.value, descriptor.maxValue);
    EXPECT_TRUE(sample.critical);
}

TEST(NeedSystemTest, UpdatesComponentsAndStoresSamples) {
    NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 2.0f;
    hunger.satisfiedThreshold = 30.0f;
    hunger.criticalThreshold = 75.0f;

    entt::registry registry;
    genesis::agents::NeedSystem system;
    system.setDefaultDescriptor(hunger);

    auto entity = registry.create();
    auto& component = registry.emplace<NeedComponent>(entity);
    system.applyDefaults(component);

    const auto* descriptor = component.needs.descriptor(NeedType::Hunger);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_FLOAT_EQ(descriptor->criticalThreshold, 75.0f);

    component.needs.setState(NeedType::Hunger, 70.0f);

    system.update(registry, 5.0f);

    auto* state = component.needs.state(NeedType::Hunger);
    ASSERT_NE(state, nullptr);
    EXPECT_FLOAT_EQ(state->value, 80.0f);

    const auto& sampleOpt = component.lastSamples[needIndex(NeedType::Hunger)];
    ASSERT_TRUE(sampleOpt.has_value());
    EXPECT_FLOAT_EQ(sampleOpt->value, state->value);

    NeedRegulator regulator;
    const auto directSample = regulator.summarise(*state, *descriptor);
    EXPECT_TRUE(directSample.critical);
}
