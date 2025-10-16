#include <gtest/gtest.h>

#include "genesis/core/SimulationClock.hpp"

using genesis::core::SimulationClock;

TEST(SimulationClock, ProducesStepsAfterAccumulation) {
    SimulationClock clock{SimulationClock::duration{100}};

    clock.advance(SimulationClock::duration{100});
    ASSERT_TRUE(clock.stepReady());

    const auto step = clock.consumeStep();
    EXPECT_EQ(step, 1);
    EXPECT_FALSE(clock.stepReady());
}

TEST(SimulationClock, RespectsMaxCatchUpLimit) {
    SimulationClock clock{SimulationClock::duration{100}, SimulationClock::duration{250}};

    clock.advance(SimulationClock::duration{500});
    EXPECT_TRUE(clock.stepReady());

    clock.consumeStep();
    EXPECT_TRUE(clock.stepReady());
    clock.consumeStep();

    EXPECT_FALSE(clock.stepReady());
    EXPECT_LT(clock.accumulated(), SimulationClock::duration{150});
}
