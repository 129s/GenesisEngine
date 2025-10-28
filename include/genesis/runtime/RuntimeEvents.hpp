#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Genesis::Runtime {

class Runtime;
class SimulationService;
struct RuntimeEventReport;

enum class RuntimeEventKind {
    Command,
    Marker
};

struct RuntimeEvent {
    std::uint64_t id{0};
    RuntimeEventKind kind{RuntimeEventKind::Command};
    std::string label;
    std::optional<std::string> payloadJson;
    std::chrono::steady_clock::time_point enqueuedAt{};
    std::function<void(SimulationService&)> simulationHandler;
    std::function<void(Runtime&)> runtimeHandler;
    std::function<void(RuntimeEventReport&)> onComplete;
};

struct RuntimeEventReport {
    std::uint64_t id{0};
    RuntimeEventKind kind{RuntimeEventKind::Command};
    std::string label;
    std::optional<std::string> payloadJson;
    std::chrono::steady_clock::time_point enqueuedAt{};
    std::chrono::steady_clock::time_point executedAt{};
    bool success{false};
    std::string message;
};

} // namespace Genesis::Runtime
