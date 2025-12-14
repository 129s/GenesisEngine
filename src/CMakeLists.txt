option(GENESIS_WITH_STYLE "Build style/palette system" OFF)
option(GENESIS_BUILD_GUI "Build sandbox GUI application" OFF)

if(GENESIS_WITH_STYLE)
    set(GENESIS_PALETTE_DATA_SOURCES
        engine/style/PaletteLoader.cpp
    )

    add_library(genesis_palette_data STATIC ${GENESIS_PALETTE_DATA_SOURCES})
    add_library(Genesis::PaletteData ALIAS genesis_palette_data)

    target_include_directories(genesis_palette_data
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(genesis_palette_data
        PUBLIC
            nlohmann_json::nlohmann_json
            spdlog::spdlog_header_only
    )

    set_target_properties(genesis_palette_data PROPERTIES
        OUTPUT_NAME "genesis_palette_data"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    set(GENESIS_STYLE_SOURCES
        engine/style/ColorRegistry.cpp
    )

    add_library(genesis_style STATIC ${GENESIS_STYLE_SOURCES})
    add_library(Genesis::Style ALIAS genesis_style)

    target_include_directories(genesis_style
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(genesis_style
        PUBLIC
            Genesis::PaletteData
            spdlog::spdlog_header_only
    )

    set_target_properties(genesis_style PROPERTIES
        OUTPUT_NAME "genesis_style"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    add_executable(genesis_palette_compiler
        tools/palette_compiler/Main.cpp
    )

    target_include_directories(genesis_palette_compiler
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    target_link_libraries(genesis_palette_compiler
        PRIVATE
            Genesis::PaletteData
            nlohmann_json::nlohmann_json
            spdlog::spdlog_header_only
    )

    set(PALETTE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/palette)

    add_custom_command(
        OUTPUT
            ${PALETTE_OUTPUT_DIR}/PaletteGenerated.hpp
            ${PALETTE_OUTPUT_DIR}/PaletteGenerated.glsl
            ${PALETTE_OUTPUT_DIR}/PaletteIndex.json
        COMMAND genesis_palette_compiler --input ${CMAKE_SOURCE_DIR}/data/palette/core.json --output ${PALETTE_OUTPUT_DIR}
        DEPENDS genesis_palette_compiler ${CMAKE_SOURCE_DIR}/data/palette/core.json
        COMMENT "Generating palette artifacts"
        VERBATIM
    )

    add_custom_target(genesis_palette_assets ALL
        DEPENDS
            ${PALETTE_OUTPUT_DIR}/PaletteGenerated.hpp
            ${PALETTE_OUTPUT_DIR}/PaletteGenerated.glsl
            ${PALETTE_OUTPUT_DIR}/PaletteIndex.json
    )

    add_dependencies(genesis_style genesis_palette_assets)
endif()

set(GENESIS_RENDERING_SOURCES
    engine/rendering/ProceduralAsset.cpp
    engine/rendering/AssetPack.cpp
    engine/rendering/AssetCache.cpp
)

add_library(genesis_rendering STATIC ${GENESIS_RENDERING_SOURCES})
add_library(Genesis::Rendering ALIAS genesis_rendering)

target_include_directories(genesis_rendering
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties(genesis_rendering PROPERTIES
    OUTPUT_NAME "genesis_rendering"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# --- A1: 世界数据模型与加载器（独立于旧 WorldRegistry） ---
add_library(genesis_world_model STATIC
    world/WorldDatabase.cpp
    world/WorldDatabaseLoader.cpp
    world/WorldDatabaseSaver.cpp
    world/MapPathfinding.cpp
)
add_library(Genesis::WorldModel ALIAS genesis_world_model)

target_include_directories(genesis_world_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_world_model
    PUBLIC
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_world_model PROPERTIES
    OUTPUT_NAME "genesis_world_model"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_WORLD_SOURCES
    world/system/ResourceSystem.cpp
)

add_library(genesis_world STATIC ${GENESIS_WORLD_SOURCES})
add_library(Genesis::World ALIAS genesis_world)

target_include_directories(genesis_world
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_world
    PUBLIC
        Genesis::WorldModel
        EnTT::EnTT
        spdlog::spdlog_header_only
)

set_target_properties(genesis_world PROPERTIES
    OUTPUT_NAME "genesis_world"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_WORLDGEN_SOURCES
    engine/worldgen/ConfigLoader.cpp
    engine/worldgen/Context.cpp
    engine/worldgen/Generator.cpp
    engine/worldgen/LayoutModule.cpp
    engine/worldgen/Rng.cpp
    engine/worldgen/TopologyModule.cpp
    engine/worldgen/ValidationModule.cpp
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

set(GENESIS_BASE_SOURCES
    foundation/SimulationClock.cpp
)

add_library(genesis_base STATIC ${GENESIS_BASE_SOURCES})
add_library(Genesis::Base ALIAS genesis_base)

target_include_directories(genesis_base
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties(genesis_base PROPERTIES
    OUTPUT_NAME "genesis_base"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_AGENTS_SOURCES
    agents/ActionSystem.cpp
    agents/NeedSystem.cpp
    agents/NeedSatisfier.cpp
    agents/ProductionPlanner.cpp
)

add_library(genesis_agents STATIC ${GENESIS_AGENTS_SOURCES})
add_library(Genesis::Agents ALIAS genesis_agents)

target_include_directories(genesis_agents
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_agents
    PUBLIC
        EnTT::EnTT
        Genesis::WorldModel
)

set_target_properties(genesis_agents PROPERTIES
    OUTPUT_NAME "genesis_agents"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_SIMULATION_SOURCES
    simulation/Movement2DSystem.cpp
    simulation/SimulationContext.cpp
    simulation/TelemetryCollector.cpp
    simulation/Scheduler.cpp
    simulation/SimulationHost.cpp
)

add_library(genesis_simulation STATIC ${GENESIS_SIMULATION_SOURCES})
add_library(Genesis::Simulation ALIAS genesis_simulation)

target_include_directories(genesis_simulation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_simulation
    PUBLIC
        Genesis::Agents
        Genesis::World
        Genesis::WorldModel
        Genesis::Base
        EnTT::EnTT
        spdlog::spdlog_header_only
)

set_target_properties(genesis_simulation PROPERTIES
    OUTPUT_NAME "genesis_simulation"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(GENESIS_ENGINE_SOURCES
    engine/core/Engine.cpp
    engine/telemetry/TelemetryBuffer.cpp
)

add_library(genesis_engine STATIC ${GENESIS_ENGINE_SOURCES})
add_library(Genesis::Engine ALIAS genesis_engine)

target_include_directories(genesis_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_engine
    PUBLIC
        spdlog::spdlog_header_only
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        Genesis::WorldModel
        Genesis::Base
        Genesis::Simulation
)

set_target_properties(genesis_engine PROPERTIES
    OUTPUT_NAME "genesis_engine"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(genesis_runtime SHARED
    engine/runtime/Runtime.cpp
    engine/runtime/EngineSimulationService.cpp
    engine/runtime/SnapshotDiff.cpp
    engine/runtime/WorldAtlas.cpp
)
add_library(Genesis::Runtime ALIAS genesis_runtime)

target_include_directories(genesis_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_runtime
    PUBLIC
        Genesis::WorldModel
    PRIVATE
        Genesis::Engine
        Genesis::Worldgen
)

set_target_properties(genesis_runtime PROPERTIES
    OUTPUT_NAME "genesis_runtime"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_executable(genesis_runtime_cli
    apps/runtime_cli/Main.cpp
)

target_link_libraries(genesis_runtime_cli
    PRIVATE
        Genesis::Runtime
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_runtime_cli PROPERTIES
    OUTPUT_NAME "genesis-runtime-cli"
)


# --- Phase 0: 基础/诊断模块骨架（占位，便于后续迁移） ---
add_library(genesis_diagnostics INTERFACE)
add_library(Genesis::Diagnostics ALIAS genesis_diagnostics)
target_include_directories(genesis_diagnostics
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(genesis_diagnostics
    INTERFACE
        spdlog::spdlog_header_only
)

# --- Phase 2（占位别名）：拆分 Runtime 子域 ---
add_library(genesis_runtime_core INTERFACE)
add_library(Genesis::RuntimeCore ALIAS genesis_runtime_core)
target_link_libraries(genesis_runtime_core INTERFACE Genesis::Runtime)

add_library(genesis_runtime_snapshot INTERFACE)
add_library(Genesis::RuntimeSnapshot ALIAS genesis_runtime_snapshot)
target_link_libraries(genesis_runtime_snapshot INTERFACE Genesis::Runtime)

if (GENESIS_BUILD_GUI)
    set(GENESIS_SANDBOX_GUI_SOURCES
        apps/sandbox_gui/gui/AppHostCore.cpp
        apps/sandbox_gui/gui/AppHostLayout.cpp
        apps/sandbox_gui/gui/AppHostPanelWorld.cpp
        apps/sandbox_gui/gui/style/DesignTokens.cpp
        apps/sandbox_gui/gui/style/LayoutMetrics.cpp
        apps/sandbox_gui/gui/controller/WorldCommandController.cpp
        apps/sandbox_gui/gui/presenter/MainViewPresenters.cpp
        apps/sandbox_gui/gui/ui/UiState.cpp
        apps/sandbox_gui/gui/ui/UiContext.cpp
        apps/sandbox_gui/gui/ui/StatusBarView.cpp
        apps/sandbox_gui/gui/ui/ControlBarView.cpp
        apps/sandbox_gui/gui/ui/BrowserView.cpp
        apps/sandbox_gui/gui/ui/MainView.cpp
        apps/sandbox_gui/gui/ui/MainViewScene.cpp
        apps/sandbox_gui/gui/ui/LayoutHelpers.cpp
        apps/sandbox_gui/gui/RuntimeBridge.cpp
        apps/sandbox_gui/gui/RuntimeBridgeAtlas.cpp
        apps/sandbox_gui/gui/RuntimeBridgeCommands.cpp
    )

    add_library(genesis_sandbox_gui_lib STATIC ${GENESIS_SANDBOX_GUI_SOURCES})
    add_library(Genesis::SandboxGui ALIAS genesis_sandbox_gui_lib)

    target_include_directories(genesis_sandbox_gui_lib
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/apps/sandbox_gui/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/apps/sandbox_gui
    )

    target_link_libraries(genesis_sandbox_gui_lib
        PUBLIC
            Genesis::Runtime
            Genesis::WorldModel
            Genesis::Rendering
            spdlog::spdlog_header_only
            imgui::imgui
            glfw
    )

    if(WIN32)
        target_link_libraries(genesis_sandbox_gui_lib PUBLIC opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(genesis_sandbox_gui_lib PUBLIC OpenGL::GL)
    endif()

    add_executable(genesis_sandbox_gui
        apps/sandbox_gui/Main.cpp
    )

    target_link_libraries(genesis_sandbox_gui
        PRIVATE
            Genesis::SandboxGui
    )

    set_target_properties(genesis_sandbox_gui PROPERTIES
        OUTPUT_NAME "genesis-sandbox-gui"
    )
endif()
