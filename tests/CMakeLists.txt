include(GoogleTest)

if(GENESIS_WITH_STYLE)
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
endif()

if(TARGET genesis_palette_compiler)
    set(PALETTE_GENERATED_TEST_DIR ${CMAKE_BINARY_DIR}/tests/palette_artifacts)
    add_test(
        NAME PaletteCompiler.GenerateArtifacts
        COMMAND genesis_palette_compiler --input ${CMAKE_SOURCE_DIR}/data/palette/core.json --output ${PALETTE_GENERATED_TEST_DIR} --quiet
    )
endif()

## --- Worldgen tests ---
add_executable(genesis_worldgen_tests
    test_worldgen_config.cpp
    test_worldgen_rng.cpp
    test_worldgen_topology.cpp
    test_worldgen_layout.cpp
    test_worldgen_validation.cpp
    test_worldgen_generator_interactives.cpp
)

target_link_libraries(genesis_worldgen_tests
    PRIVATE
        Genesis::Worldgen
        GTest::gtest_main
)

set(GENESIS_TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
file(TO_CMAKE_PATH "${GENESIS_TEST_SOURCE_DIR}" GENESIS_TEST_SOURCE_DIR)
target_compile_definitions(genesis_worldgen_tests
    PRIVATE
        GENESIS_TEST_SOURCE_DIR="${GENESIS_TEST_SOURCE_DIR}"
)

gtest_discover_tests(genesis_worldgen_tests)

## v2 重构：暂时移除旧引擎/运行时测试

if(GENESIS_BUILD_GUI)
    add_executable(genesis_sandbox_gui_tests
        test_sandbox_gui_presenters.cpp
        test_sandbox_gui_command_queue_e2e.cpp
    )

    target_include_directories(genesis_sandbox_gui_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/apps/sandbox_gui/include
            ${CMAKE_SOURCE_DIR}/src/apps/sandbox_gui
            ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(genesis_sandbox_gui_tests
        PRIVATE
            GTest::gtest_main
            Genesis::SandboxGui
    )

    set(GENESIS_TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    file(TO_CMAKE_PATH "${GENESIS_TEST_SOURCE_DIR}" GENESIS_TEST_SOURCE_DIR)
    target_compile_definitions(genesis_sandbox_gui_tests
        PRIVATE
            GENESIS_TEST_SOURCE_DIR="${GENESIS_TEST_SOURCE_DIR}"
    )

    gtest_discover_tests(genesis_sandbox_gui_tests)
endif()

## 同步移除依赖运行时/引擎可执行的 E2E 测试

# --- Runtime tests ---
add_executable(genesis_runtime_tests
    test_runtime_events.cpp
    test_runtime_simulation_service.cpp
    test_runtime_e2e_smoke.cpp
    test_runtime_emergence_smoke.cpp
    test_runtime_multi_agent_regression.cpp
    test_runtime_world_atlas.cpp
    test_runtime_json_commands.cpp
)

target_link_libraries(genesis_runtime_tests
    PRIVATE
        Genesis::Runtime
        GTest::gtest_main
)

set(GENESIS_TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
file(TO_CMAKE_PATH "${GENESIS_TEST_SOURCE_DIR}" GENESIS_TEST_SOURCE_DIR)
target_compile_definitions(genesis_runtime_tests
    PRIVATE
        GENESIS_TEST_SOURCE_DIR="${GENESIS_TEST_SOURCE_DIR}"
)

add_custom_command(TARGET genesis_runtime_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Genesis::Runtime>
        $<TARGET_FILE_DIR:genesis_runtime_tests>
)

gtest_discover_tests(genesis_runtime_tests)

# --- World Model tests ---
add_executable(genesis_world_model_tests
    test_world_database_loader.cpp
    test_world_map_pathfinding.cpp
)

target_link_libraries(genesis_world_model_tests
    PRIVATE
        Genesis::WorldModel
        GTest::gtest_main
)

gtest_discover_tests(genesis_world_model_tests)

add_executable(genesis_world_tests
    test_world_resource_system.cpp
)

target_link_libraries(genesis_world_tests
    PRIVATE
        Genesis::World
        Genesis::WorldModel
        GTest::gtest_main
)

gtest_discover_tests(genesis_world_tests)
