#pragma once

#include <cstdint>
#include <random>

#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

class DeterministicRng
{
public:
    explicit DeterministicRng(Seed seed);

    [[nodiscard]] std::uint64_t next_u64();
    [[nodiscard]] double uniform(double min, double max);
    [[nodiscard]] std::size_t uniform_index(std::size_t bound);

    [[nodiscard]] DeterministicRng fork(std::uint64_t salt) const;

private:
    std::mt19937_64 engine_;
};

} // namespace genesis::worldgen

