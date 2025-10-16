#pragma once

#include <utility>

#include <entt/entt.hpp>

namespace genesis::messaging {

class EventBus {
public:
    EventBus() = default;

    template <typename Event, typename... Args>
    void trigger(Args&&... args) {
        m_dispatcher.trigger<Event>(std::forward<Args>(args)...);
    }

    template <typename Event, typename... Args>
    void enqueue(Args&&... args) {
        m_dispatcher.enqueue<Event>(std::forward<Args>(args)...);
    }

    template <typename Event>
    void update() {
        m_dispatcher.update<Event>();
    }

    void updateAll() {
        m_dispatcher.update();
    }

    template <typename Event>
    [[nodiscard]] entt::sink<Event> sink() {
        return m_dispatcher.sink<Event>();
    }

    [[nodiscard]] entt::dispatcher& raw() noexcept { return m_dispatcher; }
    [[nodiscard]] const entt::dispatcher& raw() const noexcept { return m_dispatcher; }

private:
    entt::dispatcher m_dispatcher;
};

} // namespace genesis::messaging
