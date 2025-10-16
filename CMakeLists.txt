cmake_minimum_required(VERSION 3.21)

project(GenesisEngine VERSION 0.1.0 LANGUAGES CXX)

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
