#include "genesis/core/Engine.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main() {
    auto logger = spdlog::stdout_color_mt("genesis");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("GenesisEngine bootstrap");

    genesis::core::Engine engine;
    engine.run(10);

    spdlog::info("GenesisEngine shutdown");
    return 0;
}
