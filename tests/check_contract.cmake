execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${BUILD_CONFIG}"
          --target "${TEST_TARGET}" --parallel 2
  RESULT_VARIABLE build_result)
if (NOT build_result STREQUAL "0")
  message(FATAL_ERROR "API contract did not compile: ${TEST_TARGET}")
endif()

execute_process(COMMAND "${TEST_EXECUTABLE}" RESULT_VARIABLE test_result)
if (NOT test_result STREQUAL "0")
  message(FATAL_ERROR "API contract failed at runtime: ${TEST_TARGET}")
endif()
