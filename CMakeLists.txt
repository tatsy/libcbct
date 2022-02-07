cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)
project(LibCBCT LANGUAGES C CXX)

# ===============================================
# options
# ===============================================
option(BUILD_STATIC_LIBS "Build static libraries rather than shared libraries" OFF)

# ===============================================
# global build targets
# ===============================================
set(LIBCBCT "cbct")
set(LIBCBCT_EXE "libcbct_exe")

# ===============================================
# compiler settings
# ===============================================
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")  # enable debugging
  add_definitions("-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -O2")  # enable debugging
endif()

# ===============================================
# required packages
# ===============================================
find_package(OpenCV REQUIRED)

find_package(OpenMP)
if (OPENMP_FOUND)
  message(STATUS "OpenMP found!")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  include_directories(${OpenMP_C_INCLUDE_DIR} ${OpenMP_CXX_INCLUDE_DIR})
endif()

# ===============================================
# check CUDA
# ===============================================
find_package(CUDA 9.0)
if (CUDA_FOUND)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  set(CMAKE_CUDA_STANDARD 11)
  message(STATUS "cuFFT: ${CUDA_cufft_LIBRARY}")
  endif()

# ===============================================
# output directories
# ===============================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_DEBUG_POSTFIX "-debug")

# ===============================================
# traverse subdirectories
# ===============================================
add_subdirectory(src)
