#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/CarriedResources.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace genesis::agents {

struct WorkshopRecipeInput {
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    std::uint32_t units{1};
};

struct WorkshopRecipe {
    std::uint32_t outputUnits{1};
    std::vector<WorkshopRecipeInput> inputs{};
};

[[nodiscard]] std::optional<WorkshopRecipe> parseWorkshopRecipe(const genesis::world::Interaction& interaction);

struct ProductionPlannerConfig {
    float crossMapPenalty{500.0f};
    std::uint32_t maxPlanningRetries{2};
    int maxAcquireDepth{4};
};

class ProductionPlanner {
public:
    explicit ProductionPlanner(genesis::world::WorldDatabase& db, ProductionPlannerConfig config = {});

    struct Request {
        const components::AgentLocation2D* location{nullptr};
        const components::CarriedResources* carried{nullptr};

        genesis::world::InteractionId targetWorkshop{0};
        genesis::world::ResourceType outputType{genesis::world::ResourceType::Food};
        std::uint32_t outputAmount{0};
        std::uint32_t currentRetries{0};

        NeedType need{NeedType::Hunger};
        float reliefPerUnit{0.0f};
    };

    [[nodiscard]] std::optional<std::vector<ActionTask>> buildRecoveryPlan(const Request& request) const;

private:
    genesis::world::WorldDatabase& m_db;
    ProductionPlannerConfig m_config;
};

} // namespace genesis::agents
