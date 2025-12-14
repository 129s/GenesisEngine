cmake_minimum_required(VERSION 3.21)

project(GenesisEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GENESISENGINE_ENABLE_TESTS "Build GenesisEngine tests" ON)
option(GENESIS_BUILD_GUI "Build sandbox GUI application" OFF)
option(GENESIS_WITH_STYLE "Build style/palette system" OFF)

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

if(GENESIS_BUILD_GUI)
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

    find_package(Freetype QUIET)

    if(imgui_ADDED)
        set(IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        )

        if(Freetype_FOUND)
            list(APPEND IMGUI_SOURCES
                ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
            )
        endif()

        add_library(imgui STATIC ${IMGUI_SOURCES})
        add_library(imgui::imgui ALIAS imgui)

        target_include_directories(imgui
            PUBLIC
                ${imgui_SOURCE_DIR}
                ${imgui_SOURCE_DIR}/backends
                ${imgui_SOURCE_DIR}/misc/cpp
        )

        if(Freetype_FOUND)
            target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}/misc/freetype)
        endif()

        target_link_libraries(imgui
            PUBLIC
                glfw
        )

        if(Freetype_FOUND)
            target_link_libraries(imgui PUBLIC Freetype::Freetype)
            target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
        endif()
    endif()
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

if(GENESISENGINE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
