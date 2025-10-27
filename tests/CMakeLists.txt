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

## v2 重构：移除 worldgen 相关测试（依赖旧引擎图模型）

## v2 重构：暂时移除旧引擎/运行时测试

if(GENESIS_BUILD_GUI)
    add_executable(genesis_sandbox_gui_tests
        test_sandbox_gui_presenters.cpp
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

    gtest_discover_tests(genesis_sandbox_gui_tests)
endif()

## 同步移除依赖运行时/引擎可执行的 E2E 测试

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
