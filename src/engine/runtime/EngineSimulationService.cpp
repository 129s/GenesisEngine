#include "EngineSimulationService.hpp"

namespace Genesis::Runtime {

EngineSimulationService::EngineSimulationService()
    : m_engine() {}

void EngineSimulationService::setSnapshotCallback(SnapshotCallback callback) {
    m_engine.setSnapshotCallback(std::move(callback));
}

void EngineSimulationService::step(std::uint64_t steps) {
    m_engine.step(steps);
}

void EngineSimulationService::run(std::uint64_t steps) {
    m_engine.run(steps);
}

world::WorldDbLoadResult EngineSimulationService::loadWorld(const std::filesystem::path& folder) {
    return m_engine.loadWorldFromFile(folder);
}

std::shared_ptr<world::WorldDatabase> EngineSimulationService::worldDatabase() const noexcept {
    return m_engine.worldDatabase();
}

std::uint32_t EngineSimulationService::createAgent(const simulation::AgentSpawnParams2D& params) {
    return m_engine.createAgent(params);
}

bool EngineSimulationService::setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) {
    return m_engine.setAgentMovementIntent(entityId, command);
}

bool EngineSimulationService::clearAgentMovementIntent(std::uint32_t entityId) {
    return m_engine.clearAgentMovementIntent(entityId);
}

bool EngineSimulationService::teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) {
    return m_engine.teleportAgent(entityId, target);
}

bool EngineSimulationService::deleteAgent(std::uint32_t entityId) {
    return m_engine.deleteAgent(entityId);
}

bool EngineSimulationService::agentExists(std::uint32_t entityId) const {
    return m_engine.agentExists(entityId);
}

std::optional<simulation::AgentPose2D> EngineSimulationService::queryAgentPose(std::uint32_t entityId) const {
    return m_engine.queryAgentPose(entityId);
}

std::uint32_t EngineSimulationService::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_engine.consumeResource(interactionId, amount);
}

} // namespace Genesis::Runtime
