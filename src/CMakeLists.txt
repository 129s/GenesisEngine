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
        spdlog::spdlog
        EnTT::EnTT
        nlohmann_json::nlohmann_json
)

set_target_properties(genesis_engine PROPERTIES
    OUTPUT_NAME "genesis_engine"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_executable(genesis_engine_app
    main.cpp
)

target_link_libraries(genesis_engine_app
    PRIVATE
        Genesis::Engine
        spdlog::spdlog
)

set_target_properties(genesis_engine_app PROPERTIES
    OUTPUT_NAME "genesis-engine"
)


