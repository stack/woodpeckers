add_executable(DummyTest DummyTest.cpp)
target_link_libraries(DummyTest PUBLIC Woodpeckers GTest::GTest GTest::Main)
gtest_discover_tests(DummyTest)
