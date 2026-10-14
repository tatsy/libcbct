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

if (LIBCBCT_WITH_SLANG)
  add_executable(cbct_slang_kernel_tests test_slang_kernels.cpp)
  add_dependencies(cbct_slang_kernel_tests cbct_slang_cpu_kernels)
  target_include_directories(cbct_slang_kernel_tests PRIVATE "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
  target_link_libraries(cbct_slang_kernel_tests PRIVATE ${LIBCBCT} GTest::gtest_main)
  gtest_discover_tests(cbct_slang_kernel_tests DISCOVERY_MODE PRE_TEST)
endif()
