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
        GTest::gtest_main
)

include(GoogleTest)
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
    test_noise_grid_generator.cpp
)

target_link_libraries(genesis_engine_tests
    PRIVATE
        Genesis::Engine
        GTest::gtest_main
)

gtest_discover_tests(genesis_engine_tests)

add_executable(genesis_runtime_tests
    test_runtime.cpp
)

target_link_libraries(genesis_runtime_tests
    PRIVATE
        Genesis::Runtime
        Genesis::Engine
        GTest::gtest_main
)

add_custom_command(TARGET genesis_runtime_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:genesis_runtime> $<TARGET_FILE_DIR:genesis_runtime_tests>
)

add_test(
    NAME GenesisRuntime_Smoke
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}/tests;${CMAKE_BINARY_DIR}/src;$ENV{PATH}" genesis_runtime_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

set(GENESIS_ENGINE_APP_EXECUTABLE ${CMAKE_BINARY_DIR}/src/genesis-engine${CMAKE_EXECUTABLE_SUFFIX})
set(GENESIS_SANDBOX_CLI_EXECUTABLE ${CMAKE_BINARY_DIR}/src/genesis-sandbox-cli${CMAKE_EXECUTABLE_SUFFIX})

add_test(
    NAME GenesisEngine_E2E
    COMMAND ${GENESIS_ENGINE_APP_EXECUTABLE} --steps=60 --telemetry-file ${CMAKE_BINARY_DIR}/telemetry_e2e.json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(
    NAME GenesisSandboxCLI_Script
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}/src;$ENV{PATH}" ${GENESIS_SANDBOX_CLI_EXECUTABLE} --layout ${CMAKE_SOURCE_DIR}/data/ascii_layout.json --commands "step 5;pause;resume;step 3;quit" --no-clear
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(GenesisSandboxCLI_Script PROPERTIES DEPENDS GenesisEngine_E2E)
