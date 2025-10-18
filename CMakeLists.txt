cmake_minimum_required(VERSION 3.21)

project(GenesisEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GENESISENGINE_ENABLE_TESTS "Build GenesisEngine tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.12.0
    OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_FMT_EXTERNAL OFF"
)

CPMAddPackage(
    NAME entt
    GITHUB_REPOSITORY skypjack/entt
    VERSION 3.13.0
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.3
    OPTIONS "JSON_Install OFF"
)

CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    VERSION 3.4.0
    OPTIONS "TOMLPP_BUILD_EXAMPLES OFF" "TOMLPP_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.8
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "GLFW_INSTALL OFF"
)

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.91.0-docking
)

if(imgui_ADDED)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    add_library(imgui::imgui ALIAS imgui)

    target_include_directories(imgui
        PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${imgui_SOURCE_DIR}/misc/cpp
    )

    target_link_libraries(imgui
        PUBLIC
            glfw
    )
endif()

if(GENESISENGINE_ENABLE_TESTS)
    CPMAddPackage(
        NAME GTest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.14.0
        OPTIONS "INSTALL_GTEST OFF"
    )
endif()

add_subdirectory(src)

set(GENESIS_NOISE_GENERATOR_EXECUTABLE
    "${CMAKE_BINARY_DIR}/src/genesis-noise-generator${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_target(generate_noise_world
    COMMAND ${GENESIS_NOISE_GENERATOR_EXECUTABLE}
        --seed=1337
        --width=64
        --height=64
        --threshold=0.5
        --soil-density=0.05
        --capacity=24
        --rate=3
        --world=data/world/generated/noise_mvp.json
        --layout=data/world/generated/noise_mvp_layout.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating deterministic noise world data (seed=1337)"
)

add_dependencies(generate_noise_world genesis_noise_generator)

if(GENESISENGINE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
