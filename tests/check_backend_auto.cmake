# Hide Vulkan ICDs for this process, including with older loaders.
set(ENV{VK_DRIVER_FILES} "${CMAKE_CURRENT_BINARY_DIR}/__missing_vulkan_driver__.json")
set(ENV{VK_ICD_FILENAMES} "$ENV{VK_DRIVER_FILES}")
execute_process(
  COMMAND "${CBCT_EXE}" --backend auto
          --config "${CMAKE_CURRENT_BINARY_DIR}/__missing_auto_backend_config__.json"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
# CUDA can still be selected on CUDA-capable hosts; otherwise auto must use CPU.
if (NOT result EQUAL 1 OR NOT "${output}${error}" MATCHES "Reconstruction backend: (cpu|cuda)"
    OR NOT "${output}${error}" MATCHES "failed to open file")
  message(FATAL_ERROR "Automatic backend selection failed without Vulkan: ${result}\n${output}\n${error}")
endif()
