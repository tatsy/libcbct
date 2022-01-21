if (BUILD_STATIC_LIBS)
  add_library(${LIBCBCT} STATIC)
else()
  add_library(${LIBCBCT} SHARED)
  target_link_libraries(
    ${LIBCBCT}
    ${OpenCV_LIBS}
    ${OpenMP_libomp_LIBRARY})
endif()

add_subdirectory(Common)
add_subdirectory(Geometry)
add_subdirectory(IO)
add_subdirectory(Reconstruction)
add_subdirectory(Settings)
add_subdirectory(Utils)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCV_INCLUDE_DIRS})

add_executable(${LIBCBCT_EXE} main.cpp)
target_link_libraries(
  ${LIBCBCT_EXE}
  ${OpenCV_LIBS}
  ${OpenMP_libomp_LIBRARY})
set_property(TARGET ${BUILD_TARGET} PROPERTY DEBUG_POSTFIX "-debug")

if (MSVC)
  set_property(TARGET ${BUILD_TARGET} APPEND PROPERTY LINK_FLAGS "/DEBUG /PROFILE")
endif()
