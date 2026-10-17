# CUDA selection must succeed before any input or PTX is loaded.
set(backend "cuda")
execute_process(
  COMMAND "${CBCT_EXE}" --backend "${backend}"
          --config "${CMAKE_CURRENT_BINARY_DIR}/__missing_cuda_backend_config__.json"
          --ptx-dir "${CMAKE_CURRENT_BINARY_DIR}/__missing_ptx__"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if (NOT result EQUAL 1 OR NOT "${output}${error}" MATCHES "Reconstruction backend: cuda"
    OR NOT "${output}${error}" MATCHES "failed to open file")
  message(FATAL_ERROR "Backend ${backend} did not select CUDA PTX: ${result}\n${output}\n${error}")
endif()
