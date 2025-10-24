include(GoogleTest)

add_executable(genesis_style_tests
    test_palette.cpp
)

target_link_libraries(genesis_style_tests
    PRIVATE
        Genesis::Style
        GTest::gtest_main
)

add_dependencies(genesis_style_tests genesis_palette_assets)

gtest_discover_tests(genesis_style_tests)

set(PALETTE_GENERATED_TEST_DIR ${CMAKE_BINARY_DIR}/tests/palette_artifacts)

add_test(
    NAME PaletteCompiler.GenerateArtifacts
    COMMAND genesis_palette_compiler --input ${CMAKE_SOURCE_DIR}/data/palette/core.json --output ${PALETTE_GENERATED_TEST_DIR} --quiet
)

add_executable(genesis_worldgen_tests
    test_worldgen_config.cpp
    test_worldgen_rng.cpp
    test_worldgen_layout.cpp
    test_worldgen_validation.cpp
    test_worldgen_topology.cpp
    test_worldgen_generator.cpp
)

target_link_libraries(genesis_worldgen_tests
    PRIVATE
        Genesis::Worldgen
        Genesis::Engine
        GTest::gtest_main
)

gtest_discover_tests(genesis_worldgen_tests)

add_executable(genesis_engine_tests
    test_simulation_clock.cpp
    test_world_registry.cpp
    test_needs.cpp
    test_resource_system.cpp
    test_need_satisfier.cpp
    test_hunger_planner.cpp
    test_telemetry.cpp
    test_movement_system.cpp
    test_action_executor.cpp
)

target_link_libraries(genesis_engine_tests
    PRIVATE
        Genesis::Engine
        Genesis::Worldgen
        GTest::gtest_main
)

gtest_discover_tests(genesis_engine_tests)

add_executable(genesis_runtime_tests
    test_runtime.cpp
    test_runtime_snapshot_diff.cpp
    test_runtime_events.cpp
)

target_link_libraries(genesis_runtime_tests
    PRIVATE
        Genesis::Runtime
        Genesis::Engine
        GTest::gtest_main
)

add_executable(genesis_sandbox_gui_tests
    test_sandbox_gui_presenters.cpp
    ../src/sandbox/gui/presenter/MainViewPresenters.cpp
)

target_include_directories(genesis_sandbox_gui_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(genesis_sandbox_gui_tests
    PRIVATE
        GTest::gtest_main
        imgui::imgui
        tomlplusplus::tomlplusplus
)

gtest_discover_tests(genesis_sandbox_gui_tests)

add_custom_command(TARGET genesis_runtime_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:genesis_runtime> $<TARGET_FILE_DIR:genesis_runtime_tests>
)

add_test(
    NAME GenesisRuntime_Smoke
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}/tests;${CMAKE_BINARY_DIR}/src;$ENV{PATH}" genesis_runtime_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

set(GENESIS_ENGINE_APP_EXECUTABLE ${CMAKE_BINARY_DIR}/src/genesis-engine${CMAKE_EXECUTABLE_SUFFIX})
add_test(
    NAME GenesisEngine_E2E
    COMMAND ${GENESIS_ENGINE_APP_EXECUTABLE} --steps=60 --telemetry-file ${CMAKE_BINARY_DIR}/telemetry_e2e.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# --- World Model tests ---
add_executable(genesis_world_model_tests
    test_world_database_loader.cpp
)

target_link_libraries(genesis_world_model_tests
    PRIVATE
        Genesis::WorldModel
        GTest::gtest_main
)

gtest_discover_tests(genesis_world_model_tests)
