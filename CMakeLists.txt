cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(LibCBCT LANGUAGES CXX)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ===============================================
# Options
# ===============================================
option(LIBCBCT_WITH_CUDA "Build with CUDA support" OFF)
option(LIBCBCT_WITH_VULKAN "Build with Vulkan compute and reconstruction support" OFF)
option(LIBCBCT_WITH_OPENMP "Build with OpenMP support" OFF)
option(LIBCBCT_BUILD_TESTS "Build tests" OFF)
option(LIBCBCT_BUILD_STATIC_LIBS "Build static libraries rather than shared libraries" OFF)
option(LIBCBCT_BUILD_OPENCV_FROM_SOURCE "Build OpenCV from source" OFF)

# Slang is a dependency of either GPU backend, not an independent build option.
# Remove the former option from existing build trees before deriving its value.
unset(LIBCBCT_WITH_SLANG CACHE)
set(LIBCBCT_WITH_SLANG OFF)
if (LIBCBCT_WITH_CUDA OR LIBCBCT_WITH_VULKAN)
  set(LIBCBCT_WITH_SLANG ON)
endif()

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
  find_package(CUDAToolkit REQUIRED)
  if (NOT TARGET CUDA::nvrtc)
    message(FATAL_ERROR "The CUDA backend requires NVRTC to compile Slang kernels to PTX")
  endif()
  get_target_property(LIBCBCT_CUFFT_LIBRARY CUDA::cufft IMPORTED_LOCATION)
  message(STATUS "Building CUDA backend with Slang PTX")
  message(STATUS "  CUDA toolkit version: ${CUDAToolkit_VERSION}")
  message(STATUS "  cuFFT: ${LIBCBCT_CUFFT_LIBRARY}")
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
# Vulkan
# ===============================================
if (LIBCBCT_WITH_VULKAN)
  include(cmake/VulkanDependencies.cmake)
endif()

# ===============================================
# Slang
# ===============================================
if (LIBCBCT_WITH_SLANG)
  # Keep dependency settings local to avoid changing LibCBCT's build options.
  function(libcbct_setup_slang)
    set(slang_ROOT "" CACHE PATH "Root directory of a local Slang installation")
    set(slang_hints "${slang_ROOT}" "$ENV{slang_ROOT}" "$ENV{SLANG_ROOT}" "$ENV{VULKAN_SDK}")

    # Prefer Slang's installed CMake package when available.
    find_package(slang QUIET CONFIG HINTS ${slang_hints})
    if (TARGET slang::slang)
      message(STATUS "Using local Slang CMake package: ${slang_DIR}")
      return()
    endif()

    # Binary distributions such as the Vulkan SDK may not include a CMake package.
    find_program(SLANGC_EXECUTABLE NAMES slangc HINTS ${slang_hints} PATH_SUFFIXES bin)
    if (SLANGC_EXECUTABLE)
      get_filename_component(slang_bin_dir "${SLANGC_EXECUTABLE}" DIRECTORY)
      get_filename_component(slang_prefix "${slang_bin_dir}" DIRECTORY)
      list(APPEND slang_hints "${slang_prefix}")
    endif()
    find_path(SLANG_INCLUDE_DIR NAMES slang.h HINTS ${slang_hints}
      PATH_SUFFIXES include include/slang)
    find_library(SLANG_LIBRARY NAMES slang slang-compiler HINTS ${slang_hints}
      PATH_SUFFIXES lib lib64)
    if (WIN32)
      find_file(SLANG_RUNTIME_LIBRARY NAMES slang.dll slang-compiler.dll
        HINTS "${slang_bin_dir}" ${slang_hints} PATH_SUFFIXES bin)
    endif()

    if (SLANGC_EXECUTABLE AND SLANG_INCLUDE_DIR AND SLANG_LIBRARY
        AND (NOT WIN32 OR SLANG_RUNTIME_LIBRARY))
      add_library(slang::slang SHARED IMPORTED)
      set_target_properties(slang::slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}")
      if (WIN32)
        set_target_properties(slang::slang PROPERTIES
          IMPORTED_IMPLIB "${SLANG_LIBRARY}"
          IMPORTED_LOCATION "${SLANG_RUNTIME_LIBRARY}")
      else()
        set_target_properties(slang::slang PROPERTIES
          IMPORTED_LOCATION "${SLANG_LIBRARY}")
      endif()
      set(SLANG_EXECUTABLE "${SLANGC_EXECUTABLE}" CACHE FILEPATH "Path to the slangc executable")
      message(STATUS "Using local Slang: ${SLANGC_EXECUTABLE}")
      message(STATUS "  Slang compiler API: ${SLANG_LIBRARY}")
      return()
    endif()

    message(STATUS "Local Slang not found. Building Slang from source...")
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
    # The compiler loads this helper at runtime rather than linking it directly.
    add_dependencies(slang slang-glslang)
    # Use the same API target name as Slang's installed CMake package.
    if (NOT TARGET slang::slang)
      add_library(slang::slang ALIAS slang)
    endif()
  endfunction()

  libcbct_setup_slang()

  if (NOT TARGET slangc)
    find_program(SLANGC_EXECUTABLE NAMES slangc
      HINTS "${slang_ROOT}" "$ENV{slang_ROOT}"
            "$ENV{SLANG_ROOT}" "$ENV{VULKAN_SDK}"
      PATH_SUFFIXES bin
      REQUIRED)
    add_executable(slangc IMPORTED)
    set_target_properties(slangc PROPERTIES
      IMPORTED_LOCATION "${SLANGC_EXECUTABLE}")
  endif()

  include(cmake/SlangKernels.cmake)
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
  set(BUILD_PERF_TESTS OFF CACHE INTERNAL "")
  set(BUILD_JAVA OFF CACHE INTERNAL "")
  set(BUILD_opencv_python2 OFF CACHE INTERNAL "")
  set(BUILD_opencv_python3 OFF CACHE INTERNAL "")
  set(BUILD_opencv_apps OFF CACHE INTERNAL "")
  set(WITH_IPP OFF CACHE INTERNAL "")
  set(WITH_VTK OFF CACHE INTERNAL "")
  set(WITH_OPENCL OFF CACHE INTERNAL "")
  set(WITH_FFMPEG OFF CACHE INTERNAL "")
  set(WITH_GSTREAMER OFF CACHE INTERNAL "")
  set(WITH_MSMF OFF CACHE INTERNAL "")          # Windows
  set(WITH_DSHOW OFF CACHE INTERNAL "")         # Windows
  set(WITH_V4L OFF CACHE INTERNAL "")           # Linux
  set(WITH_AVFOUNDATION OFF CACHE INTERNAL "")  # macOS

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
