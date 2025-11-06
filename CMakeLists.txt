cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(LibCBCT LANGUAGES C CXX)

include(CheckLanguage)
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ===============================================
# Options
# ===============================================
option(LIBCBCT_WITH_CUDA "Build with CUDA support" OFF)
option(LIBCBCT_WITH_OPENMP "Build with OpenMP support" OFF)
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
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  add_definitions("-DNOMINMAX")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
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
    ${opencv_SOURCE_DIR}/modules/imgproc/include
    ${opencv_SOURCE_DIR}/modules/imgcodecs/include
  )
endif()

# ===============================================
# OpenMP
# ===============================================

if (LIBCBCT_WITH_OPENMP)
  find_package(OpenMP REQUIRED)
  if (OPENMP_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_FLAGS}")
  endif()
endif()

# ===============================================
# CUDA
# ===============================================
if (LIBCBCT_WITH_CUDA)
  check_language(CUDA)
  if (CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_definitions("-DLIBCBCT_WITH_CUDA")

    set(CMAKE_CUDA_STANDARD 11)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    message(STATUS "cuFFT: ${CUDA_cufft_LIBRARY}")

    message(STATUS "INSTALLING EXTENSIONS WITH CUDA!")
    string(REGEX REPLACE ".[0-9][0-9]|\\." "" CUDA_V ${CMAKE_CUDA_COMPILER_VERSION})
    message(STATUS "CMAKE_CUDA_COMPILER = ${CMAKE_CUDA_COMPILER}")
    message(STATUS "CMAKE_CUDA_COMPILER_ID = ${CMAKE_CUDA_COMPILER_ID}")
    message(STATUS "CMAKE_CUDA_COMPILER_VERSION = ${CUDA_V}")
  else()
    message(WARNING "NO CUDA INSTALLATION FOUND, TRYING TO INSTALL CPU VERSION ONLY!")
    set(LIBCBCT_WITH_CUDA OFF)
  endif()
endif()



# ===============================================
# Submodules
# ===============================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/submodules/cxxopts/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/submodules/jsonhpp/include)

# ===============================================
# Traverse subdirectories
# ===============================================
add_subdirectory(src)
