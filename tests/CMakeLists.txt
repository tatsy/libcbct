include(GoogleTest)

add_executable(cbct_tests
  test_filter.cpp
  test_geometry.cpp
  test_fdk.cpp
  test_errors.cpp)

target_include_directories(cbct_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cbct_tests PRIVATE ${LIBCBCT} GTest::gtest_main)

# PRE_TEST: list tests when ctest runs, not at build time (the DLLs may not be on PATH while building)
gtest_discover_tests(cbct_tests DISCOVERY_MODE PRE_TEST)
