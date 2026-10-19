include(GoogleTest)

add_executable(cbct_tests
  test_filter.cpp
  test_geometry.cpp
  test_namespace.cpp
  test_volume.cpp
  test_normalization.cpp
  test_progress.cpp
  test_projection_sequence.cpp
  test_fdk.cpp
  test_vec.cpp
  test_errors.cpp)

target_include_directories(cbct_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(cbct_tests PRIVATE ${LIBCBCT} GTest::gtest_main opencv_core opencv_imgcodecs)

if (LIBCBCT_WITH_CUDA)
  target_sources(cbct_tests PRIVATE test_cuda_ptx.cpp test_fdk_cuda.cpp)
  target_link_libraries(cbct_tests PRIVATE CUDA::cuda_driver)
  add_dependencies(cbct_tests ${LIBCBCT_EXE})
  target_compile_definitions(cbct_tests PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
  add_test(NAME BackendCli.CudaPtxAlias
    COMMAND ${CMAKE_COMMAND} -DCBCT_EXE=$<TARGET_FILE:${LIBCBCT_EXE}>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cuda_alias.cmake")
endif()

# PRE_TEST: list tests when ctest runs, not at build time (the DLLs may not be on PATH while building)
gtest_discover_tests(cbct_tests DISCOVERY_MODE PRE_TEST)

if (LIBCBCT_WITH_VULKAN)
  add_executable(cbct_vulkan_tests test_vulkan.cpp test_vulkan_compute.cpp)
  target_link_libraries(cbct_vulkan_tests PRIVATE ${LIBCBCT} cbct_vma GTest::gtest_main)
  gtest_discover_tests(cbct_vulkan_tests DISCOVERY_MODE PRE_TEST)
  target_sources(cbct_tests PRIVATE test_fdk_vulkan.cpp)
  target_compile_definitions(cbct_tests PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
endif()

if (LIBCBCT_WITH_SLANG)
  add_executable(cbct_slang_kernel_tests test_slang_kernels.cpp test_slang_layout.cpp)
  add_dependencies(cbct_slang_kernel_tests cbct_slang_cpu_kernels cbct_slang_kernels)
  target_include_directories(cbct_slang_kernel_tests PRIVATE "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}"
    "${PROJECT_SOURCE_DIR}/submodules/jsonhpp/include")
  target_compile_definitions(cbct_slang_kernel_tests PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
  target_link_libraries(cbct_slang_kernel_tests PRIVATE ${LIBCBCT} GTest::gtest_main)
  gtest_discover_tests(cbct_slang_kernel_tests DISCOVERY_MODE PRE_TEST)
endif()
