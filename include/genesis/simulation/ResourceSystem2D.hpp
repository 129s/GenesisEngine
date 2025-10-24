#pragma once

#include <cstdint>
#include <unordered_map>

namespace genesis {
namespace world { class WorldDatabase; using InteractionId = std::uint32_t; }
namespace simulation {

class ResourceSystem2D {
public:
    struct ResourceState { std::uint32_t capacity{100}; std::uint32_t current{100}; std::uint32_t regenPerStep{1}; };

    void clear();
    void initializeFromDatabase(const world::WorldDatabase& db);
    void update(); // 简单按步再生

    std::uint32_t consume(world::InteractionId id, std::uint32_t amount);
    [[nodiscard]] const std::unordered_map<world::InteractionId, ResourceState>& states() const noexcept { return states_; }

private:
    std::unordered_map<world::InteractionId, ResourceState> states_;
};

} // namespace simulation
} // namespace genesis

