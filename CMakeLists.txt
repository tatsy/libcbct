cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)
project(LibCBCT LANGUAGES C CXX)

# ===============================================
# options
# ===============================================
option(WITH_CUDA "Build with CUDA support" OFF)
option(WITH_OPENMP "Build with OpenMP support" OFF)
option(BUILD_STATIC_LIBS "Build static libraries rather than shared libraries" OFF)

# ===============================================
# global build targets
# ===============================================
set(LIBCBCT "cbct")
set(LIBCBCT_EXE "cbct_exe")

# ===============================================
# compiler settings
# ===============================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")  # enable debugging
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  add_definitions("-DNOMINMAX")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
endif()

# ===============================================
# required packages
# ===============================================
find_package(OpenCV REQUIRED)

if (WITH_OPENMP)
  find_package(OpenMP)
  if (OPENMP_FOUND)
    message(STATUS "OpenMP found!")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    include_directories(${OpenMP_C_INCLUDE_DIR} ${OpenMP_CXX_INCLUDE_DIR})
  endif()
endif()

# ===============================================
# check CUDA
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
# output directories
# ===============================================
foreach(CONFIG_TYPE Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${CONFIG_TYPE} CONFIG)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
endforeach()
set(CMAKE_DEBUG_POSTFIX "-debug")

# ===============================================
# traverse subdirectories
# ===============================================
add_subdirectory(src)
