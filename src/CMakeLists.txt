set(GENESIS_PALETTE_DATA_SOURCES
    style/PaletteLoader.cpp
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
    style/ColorRegistry.cpp
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

set(GENESIS_RENDERING_SOURCES
    rendering/ProceduralAsset.cpp
    rendering/AssetPack.cpp
    rendering/AssetCache.cpp
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

set(GENESIS_WORLDGEN_SOURCES
    worldgen/ConfigLoader.cpp
    worldgen/Context.cpp
    worldgen/Generator.cpp
    worldgen/LayoutModule.cpp
    worldgen/TilemapModule.cpp
    worldgen/Rng.cpp
    worldgen/TopologyModule.cpp
    worldgen/ValidationModule.cpp
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
        spdlog::spdlog_header_only
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
    runtime/SnapshotDiff.cpp
)
add_library(Genesis::Runtime ALIAS genesis_runtime)

target_include_directories(genesis_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_runtime
    PUBLIC
        Genesis::Worldgen
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
        spdlog::spdlog_header_only
)

set_target_properties(genesis_engine_app PROPERTIES
    OUTPUT_NAME "genesis-engine"
)

add_executable(genesis_sandbox_gui
    apps/sandbox_gui/Main.cpp
    sandbox/gui/AppHostCore.cpp
    sandbox/gui/AppHostLayout.cpp
    sandbox/gui/AppHostPanelWorld.cpp
    sandbox/gui/style/DesignTokens.cpp
    sandbox/gui/style/LayoutMetrics.cpp
    sandbox/gui/controller/WorldCommandController.cpp
    sandbox/gui/presenter/MainViewPresenters.cpp
    sandbox/gui/ui/UiState.cpp
    sandbox/gui/ui/UiContext.cpp
    sandbox/gui/ui/StatusBarView.cpp
    sandbox/gui/ui/ControlBarView.cpp
    sandbox/gui/ui/BrowserView.cpp
    sandbox/gui/ui/MainView.cpp
    sandbox/gui/ui/LayoutHelpers.cpp
    sandbox/gui/RuntimeBridge.cpp
)

target_include_directories(genesis_sandbox_gui
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(genesis_sandbox_gui
    PRIVATE
        Genesis::Style
        Genesis::Runtime
        Genesis::Rendering
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
