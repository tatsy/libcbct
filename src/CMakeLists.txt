# ===============================================
# add libcbct library
# ===============================================
if (BUILD_STATIC_LIBS)
  add_library(${LIBCBCT} STATIC)
else()
  add_library(${LIBCBCT} SHARED)
  target_link_libraries(
    ${LIBCBCT}
    ${OpenCV_LIBS}
    ${OpenMP_libomp_LIBRARY}
    ${CUDA_LIBRARIES}
    ${CUDA_cufft_LIBRARY})  
endif()

# ===============================================
# process source directories
# ===============================================
set(SUB_DIRS
  Common;
  Geometry;
  IO;
  Reconstruction;
  Settings;
  Utils)

target_sources(${LIBCBCT} PUBLIC libcbct.h)
foreach(DIR IN LISTS SUB_DIRS)
  file(GLOB SUB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cu
    ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.h)
  target_sources(${LIBCBCT} PRIVATE ${SUB_SOURCES})
  source_group(${DIR} FILES ${SUB_SOURCES})
endforeach()

# ===============================================
# CBCT example code
# ===============================================
add_executable(${LIBCBCT_EXE} main.cpp)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCV_INCLUDE_DIRS})

target_link_libraries(
  ${LIBCBCT_EXE}
  ${LIBCBCT}
  ${OpenCV_LIBS}
  ${OpenMP_libomp_LIBRARY}
  ${CUDA_LIBRARIES}
  ${CUDA_cufft_LIBRARY})
set_property(TARGET ${LIBCBCT_EXE} PROPERTY DEBUG_POSTFIX "-debug")

if (MSVC)
  set_property(TARGET ${LIBCBCT_EXE} APPEND PROPERTY LINK_FLAGS "/DEBUG /PROFILE")
endif()
