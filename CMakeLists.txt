cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(LibCBCT LANGUAGES CXX)

include(CheckLanguage)
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ===============================================
# Options
# ===============================================
option(LIBCBCT_WITH_CUDA "Build with CUDA support" OFF)
option(LIBCBCT_WITH_SLANG "Build the Slang compiler and compiler API" OFF)
option(LIBCBCT_WITH_OPENMP "Build with OpenMP support" OFF)
option(LIBCBCT_BUILD_TESTS "Build tests" OFF)
option(LIBCBCT_BUILD_STATIC_LIBS "Build static libraries rather than shared libraries" OFF)
option(LIBCBCT_BUILD_OPENCV_FROM_SOURCE "Build OpenCV from source" OFF)

# ===============================================
# Global build targets
# ===============================================
set(LIBCBCT "cbct")
set(LIBCBCT_EXE "cbct_exe")

# ===============================================
# Compiler settings
# ===============================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (MSVC)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
  add_compile_options("$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8>")
endif()

# ===============================================
# OpenMP
# ===============================================
if (LIBCBCT_WITH_OPENMP)
  find_package(OpenMP REQUIRED)
  message(STATUS "OpenMP found: ${OpenMP_CXX_FLAGS}")
endif()

# ===============================================
# CUDA
# ===============================================
if (LIBCBCT_WITH_CUDA)
  check_language(CUDA)
  if (CMAKE_CUDA_COMPILER)
    if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
      set(CMAKE_CUDA_ARCHITECTURES "native")
    endif()

    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)

    message(STATUS "Build LibCBCT with CUDA!")
    get_target_property(LIBCBCT_CUFFT_LIBRARY CUDA::cufft IMPORTED_LOCATION)

    message(STATUS "Building with CUDA support")
    message(STATUS "  CUDA compiler: ${CMAKE_CUDA_COMPILER} (${CMAKE_CUDA_COMPILER_ID})")
    message(STATUS "  CUDA toolkit version: ${CUDAToolkit_VERSION}")
    message(STATUS "  CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")
    message(STATUS "  cuFFT: ${LIBCBCT_CUFFT_LIBRARY}")
  else()
    message(WARNING "No CUDA compiler found. Building the CPU version only!")
    set(LIBCBCT_WITH_CUDA OFF)
  endif()
endif()


# ===============================================
# Output directories
# ===============================================
foreach(CONFIG_TYPE Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${CONFIG_TYPE} CONFIG)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
endforeach()
set(CMAKE_DEBUG_POSTFIX "-debug")

# ===============================================
# Slang
# ===============================================
if (LIBCBCT_WITH_SLANG)
  # Keep dependency settings local to avoid changing LibCBCT's build options.
  function(libcbct_fetch_slang)
    # Slang loads helper libraries by their upstream filenames at runtime.
    set(CMAKE_DEBUG_POSTFIX "")
    # Only the compiler API and command-line compiler are needed for kernels.
    foreach(component GFX SLANG_RHI SLANGD SLANGI SLANGRT
                      TESTS EXAMPLES REPLAYER CUDA OPTIX NVAPI XLIB AFTERMATH DXIL)
      set(SLANG_ENABLE_${component} OFF)
    endforeach()
    set(SLANG_ENABLE_SLANGC ON)
    # SPIR-V generation uses this wrapper for optimization and validation.
    set(SLANG_ENABLE_SLANG_GLSLANG ON)
    set(SLANG_ENABLE_SPIRV_TOOLS_MIMALLOC OFF)
    set(SLANG_ENABLE_PREBUILT_BINARIES OFF)
    set(SLANG_EXCLUDE_DAWN ON)
    set(SLANG_EXCLUDE_TINT ON)
    # LLVM is only needed for Slang's CPU compilation targets.
    set(SLANG_SLANG_LLVM_FLAVOR DISABLE)
    set(SLANG_LIB_TYPE SHARED)

    FetchContent_Declare(
      slang
      GIT_REPOSITORY https://github.com/shader-slang/slang.git
      GIT_TAG v2025.24.3
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
      GIT_SUBMODULES
        external/unordered_dense
        external/miniz
        external/lz4
        external/vulkan
        external/spirv-headers
        external/spirv-tools
        external/glslang
        external/metal-cpp
        external/lua
      GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_MakeAvailable(slang)
  endfunction()

  libcbct_fetch_slang()
  # The compiler loads this helper at runtime rather than linking it directly.
  add_dependencies(slang slang-glslang)
  # Use the same API target name as Slang's installed CMake package.
  if (NOT TARGET slang::slang)
    add_library(slang::slang ALIAS slang)
  endif()
endif()

# ===============================================
# OpenCV
# ===============================================
if (NOT LIBCBCT_BUILD_OPENCV_FROM_SOURCE)
  find_package(OpenCV QUIET CONFIG)
endif()

if (OpenCV_FOUND)
  message(STATUS "OpenCV found! Version: ${OpenCV_VERSION}")
else()
  # Build OpenCV from source
  message(STATUS "Building OpenCV from source...")
  FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv
    GIT_TAG 4.12.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
  )

  set(BUILD_LIST "core;imgproc;imgcodecs;highgui" CACHE INTERNAL "")
  set(BUILD_TESTS OFF CACHE INTERNAL "")
  set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
  set(BUILD_PERF_TEST OFF CACHE INTERNAL "")
  set(BUILD_JAVA OFF CACHE INTERNAL "")
  set(BUILD_PYTHON_BINDINGS OFF CACHE INTERNAL "")
  set(WITH_IPP OFF CACHE INTERNAL "")
  set(WITH_VTK OFF CACHE INTERNAL "")

  FetchContent_MakeAvailable(opencv)
  set(OpenCV_INCLUDE_DIRS
    ${opencv_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${opencv_SOURCE_DIR}/modules/core/include
    ${opencv_SOURCE_DIR}/modules/highgui/include
    ${opencv_SOURCE_DIR}/modules/imgproc/include
    ${opencv_SOURCE_DIR}/modules/imgcodecs/include
  )
endif()

# ===============================================
# Tests
# ===============================================
if (LIBCBCT_BUILD_TESTS)

  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
  )

  # Use the shared C runtime (/MD) on MSVC to match libcbct
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_subdirectory(tests)
endif()

# ===============================================
# Traverse subdirectories
# ===============================================
add_subdirectory(src)
