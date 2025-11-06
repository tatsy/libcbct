# ===============================================
# add libcbct library
# ===============================================
if (LIBCBCT_BUILD_STATIC_LIBS)
  add_library(${LIBCBCT} STATIC)
else()
  add_library(${LIBCBCT} SHARED)
endif()

target_include_directories(
  ${LIBCBCT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  ${LIBCBCT} PUBLIC
  opencv_core
  opencv_imgproc
  opencv_imgcodecs
  opencv_highgui
)

if (LIBCBCT_WITH_OPENMP)
  target_link_libraries(
    ${LIBCBCT} PUBLIC
    OpenMP::OpenMP_CXX
  )
endif()

if (LIBCBCT_WITH_CUDA)
  target_link_libraries(
    ${LIBCBCT} PUBLIC
    ${CUDA_LIBRARIES}
    ${CUDA_cufft_LIBRARY}
  )
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
  Utils
)

target_sources(${LIBCBCT} PUBLIC libcbct.h)
foreach(DIR IN LISTS SUB_DIRS)
  file(GLOB SUB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.h)

  if (LIBCBCT_WITH_CUDA)
    file(GLOB CUDA_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cu)
    list(APPEND SUB_SOURCES ${CUDA_SOURCES})
  endif()

  target_sources(${LIBCBCT} PRIVATE ${SUB_SOURCES})
  source_group(${DIR} FILES ${SUB_SOURCES})
endforeach()

# ===============================================
# CBCT example code
# ===============================================
add_executable(${LIBCBCT_EXE} main.cpp)

target_include_directories(
  ${LIBCBCT_EXE} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  ${LIBCBCT_EXE} PRIVATE
  ${LIBCBCT}
)

set_property(TARGET ${LIBCBCT_EXE} PROPERTY DEBUG_POSTFIX "-debug")

if (MSVC)
  set_property(TARGET ${LIBCBCT_EXE} APPEND PROPERTY LINK_FLAGS "/DEBUG /PROFILE")
endif()
