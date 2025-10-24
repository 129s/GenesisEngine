#include "genesis/simulation/ResourceSystem2D.hpp"

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

void ResourceSystem2D::clear() { states_.clear(); }

void ResourceSystem2D::initializeFromDatabase(const world::WorldDatabase& db) {
    states_.clear();
    for (const auto& m : db.maps()) {
        for (const auto& i : db.interactions(m.id)) {
            if (i.kind == world::InteractionKind::Resource) {
                ResourceState st{};
                if (i.capacity) { st.capacity = *i.capacity; }
                if (i.regenPerStep) { st.regenPerStep = *i.regenPerStep; }
                st.current = st.capacity; // 启动时填满
                states_.emplace(i.id, st);
            }
        }
    }
}

void ResourceSystem2D::update() {
    for (auto& [_, state] : states_) {
        const auto space = state.capacity > state.current ? (state.capacity - state.current) : 0U;
        const auto add = state.regenPerStep > space ? space : state.regenPerStep;
        state.current += add;
    }
}

std::uint32_t ResourceSystem2D::consume(world::InteractionId id, std::uint32_t amount) {
    if (amount == 0) return 0;
    auto it = states_.find(id);
    if (it == states_.end()) return 0;
    auto& st = it->second;
    const std::uint32_t take = (st.current < amount) ? st.current : amount;
    st.current -= take;
    return take;
}

} // namespace genesis::simulation
