#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "genesis/worldgen/Rng.hpp"

using namespace genesis::worldgen;

TEST(WorldgenRng, DeterministicSequence)
{
    const Seed seed{12345};
    DeterministicRng rng_a(seed);
    DeterministicRng rng_b(seed);

    std::vector<std::uint64_t> sequence_a;
    std::vector<std::uint64_t> sequence_b;

    for (int i = 0; i < 8; ++i)
    {
        sequence_a.push_back(rng_a.next_u64());
        sequence_b.push_back(rng_b.next_u64());
    }

    EXPECT_EQ(sequence_a, sequence_b);
}

TEST(WorldgenRng, ForkProducesStableChild)
{
    DeterministicRng parent(Seed{42});
    auto child_one = parent.fork(0xBEEF);
    auto child_two = parent.fork(0xBEEF);

    const auto first = child_one.next_u64();
    const auto second = child_two.next_u64();
    EXPECT_EQ(first, second);

    DeterministicRng parent_comparison(Seed{42});
    const auto parent_direct = parent_comparison.next_u64();
    const auto fork_result = parent_comparison.fork(0xBEEF).next_u64();
    EXPECT_NE(parent_direct, fork_result);
}

TEST(WorldgenRng, UniformIndexRejectsZero)
{
    DeterministicRng rng(Seed{999});
    EXPECT_THROW(static_cast<void>(rng.uniform_index(0)), std::invalid_argument);
}
