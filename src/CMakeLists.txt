set(GENESIS_WORLDGEN_SOURCES
    worldgen/ConfigLoader.cpp
    worldgen/Context.cpp
    worldgen/Generator.cpp
    worldgen/Rng.cpp
    worldgen/TopologyModule.cpp
)

add_library(genesis_worldgen STATIC ${GENESIS_WORLDGEN_SOURCES})
add_library(Genesis::Worldgen ALIAS genesis_worldgen)

target_include_directories(genesis_worldgen
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_worldgen
    PUBLIC
        tomlplusplus::tomlplusplus
)

set_target_properties(genesis_worldgen PROPERTIES
    OUTPUT_NAME "genesis_worldgen"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_ENGINE_SOURCES
    agents/NeedSystem.cpp
    agents/NeedSatisfier.cpp
    agents/ActionSystem.cpp
    agents/MovementSystem.cpp
    core/SimulationClock.cpp
    core/Engine.cpp
    world/WorldRegistry.cpp
    world/WorldBootstrap.cpp
    world/WorldLoader.cpp
    world/generation/NoiseGridGenerator.cpp
    world/system/ResourceSystem.cpp
    planner/HungerPlanner.cpp
    telemetry/TelemetryBuffer.cpp
)

add_library(genesis_engine STATIC ${GENESIS_ENGINE_SOURCES})
add_library(Genesis::Engine ALIAS genesis_engine)

target_include_directories(genesis_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_engine
    PUBLIC
        spdlog::spdlog
        EnTT::EnTT
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_engine PROPERTIES
    OUTPUT_NAME "genesis_engine"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(genesis_runtime SHARED
    runtime/Runtime.cpp
)
add_library(Genesis::Runtime ALIAS genesis_runtime)

target_include_directories(genesis_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_runtime
    PRIVATE
        Genesis::Engine
)

set_target_properties(genesis_runtime PROPERTIES
    OUTPUT_NAME "genesis_runtime"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_executable(genesis_engine_app
    main.cpp
)

target_link_libraries(genesis_engine_app
    PRIVATE
        Genesis::Engine
        spdlog::spdlog
)

set_target_properties(genesis_engine_app PROPERTIES
    OUTPUT_NAME "genesis-engine"
)

add_executable(genesis_sandbox_cli
    apps/sandbox_cli/Main.cpp
    sandbox/CliApp.cpp
    sandbox/CliRenderer.cpp
)

target_include_directories(genesis_sandbox_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_sandbox_cli
    PRIVATE
        Genesis::Runtime
        Genesis::Engine
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_sandbox_cli PROPERTIES
    OUTPUT_NAME "genesis-sandbox-cli"
)

add_executable(genesis_sandbox_gui
    apps/sandbox_gui/Main.cpp
    sandbox/gui/AppHost.cpp
    sandbox/gui/RuntimeBridge.cpp
)

target_include_directories(genesis_sandbox_gui
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_sandbox_gui
    PRIVATE
        Genesis::Runtime
        spdlog::spdlog_header_only
        imgui::imgui
        glfw
)

if(WIN32)
    target_link_libraries(genesis_sandbox_gui PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(genesis_sandbox_gui PRIVATE OpenGL::GL)
endif()

set_target_properties(genesis_sandbox_gui PROPERTIES
    OUTPUT_NAME "genesis-sandbox-gui"
)

add_executable(genesis_noise_generator
    tools/NoiseGenMain.cpp
)

target_include_directories(genesis_noise_generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_noise_generator
    PRIVATE
        Genesis::Engine
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_noise_generator PROPERTIES
    OUTPUT_NAME "genesis-noise-generator"
)


