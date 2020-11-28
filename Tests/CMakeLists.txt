add_executable(ConfigurationTest ConfigurationTest.cpp)
target_include_directories(ConfigurationTest PRIVATE ${SOURCES_PATH})
target_link_libraries(ConfigurationTest PUBLIC Woodpeckers GTest::GTest GTest::Main)
gtest_discover_tests(ConfigurationTest)
