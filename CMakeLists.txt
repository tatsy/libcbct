cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(LibCBCT LANGUAGES C CXX)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ===============================================
# Options
# ===============================================
option(WITH_CUDA "Build with CUDA support" OFF)
option(WITH_OPENMP "Build with OpenMP support" OFF)
option(BUILD_STATIC_LIBS "Build static libraries rather than shared libraries" OFF)
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
if (MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  add_definitions("-DNOMINMAX")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
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
  set(OPENCV_BUILD_LIST "core;imgproc;imgcodecs;highgui" CACHE STRING "" FORCE)

  FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv
    GIT_TAG 4.12.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
  )

  set(BUILD_LIST ${OPENCV_BUILD_LIST} CACHE STRING "" FORCE)
  set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_PERF_TEST OFF CACHE BOOL "" FORCE)
  set(BUILD_JAVA OFF CACHE BOOL "" FORCE)
  set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
  set(WITH_VTK OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(opencv)
  find_package(OpenCV REQUIRED CONFIG)
  
  mark_as_advanced(FETCHCONTENT_BASE_DIR)
  mark_as_advanced(FETCHCONTENT_FULLY_DISCONNECTED)
  mark_as_advanced(FETCHCONTENT_QUIET)
  mark_as_advanced(FETCHCONTENT_SOURCE_DIR_opencv)
  mark_as_advanced(FETCHCONTENT_UPDATES_DISCONNECTED)
  mark_as_advanced(FETCHCONTENT_UPDATES_DISCONNECTED_opencv)
endif()

# ===============================================
# OpenMP
# ===============================================

if (WITH_OPENMP)
  find_package(OpenMP REQUIRED)
  if (OPENMP_FOUND)
    message(STATUS "OpenMP found!")
  endif()
endif()

# ===============================================
# CUDA
# ===============================================
if (WITH_CUDA)
  enable_language(CUDA)
  if (CUDA_FOUND)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_STANDARD 11)
    message(STATUS "cuFFT: ${CUDA_cufft_LIBRARY}")
  endif()
  add_definitions("-DLIBCBCT_WITH_CUDA")
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
# Submodules
# ===============================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/submodules/cxxopts/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/submodules/jsonhpp/include)

# ===============================================
# Traverse subdirectories
# ===============================================
add_subdirectory(src)
