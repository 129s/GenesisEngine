#include "genesis/worldgen/Rng.hpp"

#include <limits>
#include <stdexcept>

namespace genesis::worldgen
{

namespace
{
std::uint64_t mix_seed(std::uint64_t base, std::uint64_t salt)
{
    constexpr std::uint64_t prime = 0x9E3779B97f4A7C15ull;
    std::uint64_t result = base;
    result ^= salt + prime + (result << 6U) + (result >> 2U);
    return result;
}
} // namespace

DeterministicRng::DeterministicRng(Seed seed)
    : engine_(seed.value ? seed.value : std::mt19937_64::default_seed)
{
}

std::uint64_t DeterministicRng::next_u64()
{
    return engine_();
}

double DeterministicRng::uniform(double min, double max)
{
    if (!(min <= max))
    {
        throw std::invalid_argument("uniform 输入区间非法");
    }

    std::uniform_real_distribution<double> dist(min, max);
    return dist(engine_);
}

std::size_t DeterministicRng::uniform_index(std::size_t bound)
{
    if (bound == 0)
    {
        throw std::invalid_argument("uniform_index 需要正的上界");
    }

    std::uniform_int_distribution<std::size_t> dist(0, bound - 1);
    return dist(engine_);
}

DeterministicRng DeterministicRng::fork(std::uint64_t salt) const
{
    auto engine_copy = engine_;
    const auto derived = mix_seed(engine_copy(), salt);
    return DeterministicRng(Seed{derived});
}

} // namespace genesis::worldgen

