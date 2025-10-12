cmake_minimum_required(VERSION 3.16)
project(GenesisEngine VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 查找SDL2
find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
    message(STATUS "SDL2 include directories: ${SDL2_INCLUDE_DIRS}")
endif()

# 手动查找 SDL2_ttf 库
find_library(SDL2_TTF_LIBRARY
    NAMES SDL2_ttf
    PATHS /usr/lib /usr/local/lib
    PATH_SUFFIXES lib64 lib
)

if(SDL2_TTF_LIBRARY)
    message(STATUS "Found SDL2_ttf: ${SDL2_TTF_LIBRARY}")
else()
    message(FATAL_ERROR "SDL2_ttf not found")
endif()

# 查找EnTT
find_package(entt QUIET)
if(NOT entt_FOUND)
    # 如果系统没有安装EnTT，使用FetchContent
    message(STATUS "EnTT not found system-wide, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG        v3.13.0
    )
    FetchContent_MakeAvailable(entt)
else()
    message(STATUS "Found system EnTT")
endif()

# 包含目录
include_directories(include)
include_directories(external)

# 自动发现源文件
file(GLOB_RECURSE ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
)

# 添加可执行文件
add_executable(GenesisEngine ${ENGINE_SOURCES})

# 链接库
target_link_libraries(GenesisEngine
    SDL2::SDL2
    ${SDL2_TTF_LIBRARY}
)

# 设置输出目录
set_target_properties(GenesisEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)


# 拷贝配置文件到输出目录
add_custom_command(TARGET GenesisEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/config $<TARGET_FILE_DIR:GenesisEngine>/config
    COMMENT "Copying configuration files to output directory"
)