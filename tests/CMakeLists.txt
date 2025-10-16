add_executable(genesis_engine_tests
    test_simulation_clock.cpp
    test_world_registry.cpp
    test_needs.cpp
    test_resource_system.cpp
    test_need_satisfier.cpp
    test_telemetry.cpp
)

target_link_libraries(genesis_engine_tests
    PRIVATE
        Genesis::Engine
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(genesis_engine_tests)

