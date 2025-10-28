#include <gtest/gtest.h>

#include <entt/entt.hpp>
#include <iterator>

#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/events/ResourceEvents.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace {

class ResourceSystemTest : public ::testing::Test {
protected:
    struct EventListener {
        std::vector<genesis::world::events::ResourceConsumed> consumed;
        std::vector<genesis::world::events::ResourceLowStock> low;

        void onConsumed(const genesis::world::events::ResourceConsumed& evt) {
            consumed.push_back(evt);
        }

        void onLow(const genesis::world::events::ResourceLowStock& evt) {
            low.push_back(evt);
        }

        void clear() {
            consumed.clear();
            low.clear();
        }
    };

    void SetUp() override {
        database_.clear();
        genesis::world::Map map{};
        map.id = 1;
        map.name = "TestMap";
        database_.addMap(map);

        genesis::world::Interaction resource{};
        resource.id = kInteractionId;
        resource.mapId = map.id;
        resource.kind = genesis::world::InteractionKind::Resource;
        resource.capacity = 10;
        resource.regenPerStep = 2;
        resource.name = "Berries";
        database_.addInteraction(resource);

        system_ = std::make_unique<genesis::world::system::ResourceSystem>(database_, eventBus_);
        auto& dispatcher = eventBus_.raw();
        dispatcher.sink<genesis::world::events::ResourceConsumed>().connect<&EventListener::onConsumed>(&listener_);
        dispatcher.sink<genesis::world::events::ResourceLowStock>().connect<&EventListener::onLow>(&listener_);

        system_->initialize(registry_);
    }

    void TearDown() override {
        auto& dispatcher = eventBus_.raw();
        dispatcher.sink<genesis::world::events::ResourceConsumed>().disconnect<&EventListener::onConsumed>(&listener_);
        dispatcher.sink<genesis::world::events::ResourceLowStock>().disconnect<&EventListener::onLow>(&listener_);
        system_->reset(registry_);
        system_.reset();
        registry_.clear();
        listener_.clear();
    }

    static constexpr std::uint32_t kInteractionId = 1001;

    entt::registry registry_;
    genesis::world::InMemoryWorldDatabase database_;
    genesis::messaging::EventBus eventBus_;
    EventListener listener_;
    std::unique_ptr<genesis::world::system::ResourceSystem> system_;
};

TEST_F(ResourceSystemTest, InitializeCreatesSpawnsWithInitialInventory) {
    auto view = registry_.view<genesis::world::components::ResourceInventory,
                               genesis::world::components::ResourceSpawn>();
    ASSERT_EQ(std::distance(view.begin(), view.end()), 1);

    auto entity = *view.begin();
    const auto& inventory = view.get<genesis::world::components::ResourceInventory>(entity);
    const auto& spawn = view.get<genesis::world::components::ResourceSpawn>(entity);

    EXPECT_EQ(inventory.capacity, 10U);
    EXPECT_EQ(inventory.current, 10U);
    EXPECT_EQ(spawn.interaction, kInteractionId);
    EXPECT_EQ(spawn.ratePerStep, 2U);
    EXPECT_EQ(spawn.mapId, 1U);
    EXPECT_EQ(spawn.type, genesis::world::ResourceType::Food);
}

TEST_F(ResourceSystemTest, ConsumeEmitsEventsAndTickRegenerates) {
    constexpr std::uint32_t kRequest = 9U;
    const auto consumed = system_->consume(registry_, genesis::world::ResourceType::Food, kRequest, kInteractionId);
    EXPECT_EQ(consumed, kRequest);

    ASSERT_EQ(listener_.consumed.size(), 1U);
    const auto& consumeEvt = listener_.consumed.front();
    EXPECT_EQ(consumeEvt.interaction, kInteractionId);
    EXPECT_EQ(consumeEvt.amount, kRequest);
    EXPECT_EQ(consumeEvt.remaining, 1U);

    ASSERT_EQ(listener_.low.size(), 1U);
    const auto& lowEvt = listener_.low.front();
    EXPECT_EQ(lowEvt.interaction, kInteractionId);
    EXPECT_EQ(lowEvt.remaining, 1U);

    system_->tick(registry_, 1);

    auto view = registry_.view<genesis::world::components::ResourceInventory,
                               genesis::world::components::ResourceSpawn>();
    auto entity = *view.begin();
    const auto& inventory = view.get<genesis::world::components::ResourceInventory>(entity);

    EXPECT_EQ(inventory.current, 3U); // regenPerStep == 2, capped by capacity

    system_->reset(registry_);
    auto postResetView = registry_.view<genesis::world::components::ResourceInventory>();
    EXPECT_EQ(std::distance(postResetView.begin(), postResetView.end()), 0);
}

} // namespace
