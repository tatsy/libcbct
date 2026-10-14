set(LIBCBCT_SLANG_KERNEL_SOURCE
  "${PROJECT_SOURCE_DIR}/src/Reconstruction/Slang/FeldkampKernels.slang")
set(LIBCBCT_SLANG_KERNEL_OUTPUT_DIR "${PROJECT_BINARY_DIR}/kernels/$<CONFIG>")
set(LIBCBCT_SLANG_PTX_OPTIONS "" CACHE STRING "Additional slangc options for PTX generation (semicolon-separated)")

set(libcbct_slang_outputs)
set(libcbct_slang_ptx_outputs)
foreach(entry weightAndPadKernel applyFilterKernel backProjectionKernel)
  foreach(target cuda spirv ptx)
    set(profile_options)
    set(downstream_options)
    if (target STREQUAL "cuda")
      set(extension cu)
    elseif (target STREQUAL "spirv")
      set(extension spv)
      set(profile_options -profile spirv_1_5)
    else()
      set(extension ptx)
      set(downstream_options ${LIBCBCT_SLANG_PTX_OPTIONS})
    endif()

    set(output "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}/${entry}.${extension}")
    set(reflection "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}/${entry}.${target}.json")
    add_custom_command(
      OUTPUT "${output}" "${reflection}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}"
      COMMAND $<TARGET_FILE:slangc> "${LIBCBCT_SLANG_KERNEL_SOURCE}"
              -entry ${entry} -target ${target} ${profile_options}
              -floating-point-mode precise ${downstream_options}
              -o "${output}" -reflection-json "${reflection}"
      DEPENDS slangc "${LIBCBCT_SLANG_KERNEL_SOURCE}"
      COMMENT "Compiling ${entry} to ${target}"
      VERBATIM
    )
    if (target STREQUAL "ptx")
      list(APPEND libcbct_slang_ptx_outputs "${output}" "${reflection}")
    else()
      list(APPEND libcbct_slang_outputs "${output}" "${reflection}")
    endif()
  endforeach()
endforeach()

add_custom_target(cbct_slang_kernels ALL DEPENDS ${libcbct_slang_outputs}
                  SOURCES "${LIBCBCT_SLANG_KERNEL_SOURCE}")
# NVRTC is optional: PTX generation is requested explicitly on CUDA-equipped hosts.
add_custom_target(cbct_slang_ptx DEPENDS ${libcbct_slang_ptx_outputs})

if (LIBCBCT_BUILD_TESTS)
  set(LIBCBCT_SLANG_CPU_SOURCE "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}/FeldkampKernels.cpu.cpp")
  add_custom_command(
    OUTPUT "${LIBCBCT_SLANG_CPU_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}"
    COMMAND $<TARGET_FILE:slangc> "${LIBCBCT_SLANG_KERNEL_SOURCE}"
            -target cpp -floating-point-mode precise -o "${LIBCBCT_SLANG_CPU_SOURCE}"
    DEPENDS slangc "${LIBCBCT_SLANG_KERNEL_SOURCE}"
    COMMENT "Generating CPU code for Slang kernel comparison tests"
    VERBATIM
  )
  add_custom_target(cbct_slang_cpu_kernels DEPENDS "${LIBCBCT_SLANG_CPU_SOURCE}")
endif()
