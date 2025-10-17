#include "sandbox/gui/AppHost.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    spdlog::set_pattern("%H:%M:%S %^%l%$ [%n] %v");
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting Genesis Sandbox GUI…");

    Genesis::Sandbox::Gui::AppHost app;

    try
    {
        if (!app.initialize())
        {
            spdlog::error("Sandbox GUI initialization failed");
            return EXIT_FAILURE;
        }

        app.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("Sandbox GUI encountered a fatal error: {}", ex.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Sandbox GUI exited cleanly");
    return EXIT_SUCCESS;
}
