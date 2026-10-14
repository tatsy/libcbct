# ===============================================
# libcbct library
# ===============================================
set(LIBCBCT_HEADERS
  libcbct.h
  Common/Api.h
  Common/Constants.h
  Common/Exception.h
  Common/Logging.h
  Common/OpenMP.h
  Common/ProgressBar.h
  Geometry/GeometryBase.h
  IO/BaseExporter.h
  IO/BaseImporter.h
  IO/ImageSequenceImporter.h
  IO/RawVolumeExporter.h
  Reconstruction/FeldkampCPU.h
  Reconstruction/FilterUtils.h
  Reconstruction/ReconstructionBase.h
  Reconstruction/pocketfft_hdronly.h
  Settings/JsonSettingImporter.h
  Settings/SettingsImporterBase.h
  Utils/CudaUtils.h
  Utils/ImageUtils.h
  Utils/Vec.h
  Utils/Volume.h
)

set(LIBCBCT_SOURCES
  IO/ImageSequenceImporter.cpp
  IO/RawVolumeExporter.cpp
  Reconstruction/FeldkampCPU.cpp
  Reconstruction/FilterUtils.cpp
  Settings/JsonSettingImporter.cpp
)

if (LIBCBCT_WITH_CUDA)
  list(APPEND LIBCBCT_HEADERS Reconstruction/FeldkampCUDA.h Utils/DeviceBuffer.h Utils/CudaPtxModule.h)
  list(APPEND LIBCBCT_SOURCES Reconstruction/FeldkampCUDA.cu Utils/CudaPtxModule.cpp)
endif()

if (LIBCBCT_WITH_CUDA AND LIBCBCT_WITH_SLANG)
  list(APPEND LIBCBCT_HEADERS Reconstruction/FeldkampSlangCUDA.h Reconstruction/Slang/FdkCudaParameters.h)
  list(APPEND LIBCBCT_SOURCES Reconstruction/FeldkampSlangCUDA.cpp)
endif()

if (LIBCBCT_WITH_VULKAN)
  # Compile VMA in one translation unit, reusable by the library and Vulkan tests.
  add_library(cbct_vma STATIC Utils/VulkanMemoryAllocator.cpp)
  target_link_libraries(cbct_vma PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
  # The future device context supplies its loader's proc-address functions to VMA.
  target_compile_definitions(cbct_vma PUBLIC
    VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES Utils/VulkanMemoryAllocator.cpp)
endif()

if (LIBCBCT_BUILD_STATIC_LIBS)
  add_library(${LIBCBCT} STATIC ${LIBCBCT_SOURCES} ${LIBCBCT_HEADERS})
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_STATIC)
else()
  add_library(${LIBCBCT} SHARED ${LIBCBCT_SOURCES} ${LIBCBCT_HEADERS})
  set_target_properties(${LIBCBCT} PROPERTIES DEFINE_SYMBOL LIBCBCT_API_EXPORT)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LIBCBCT_SOURCES} ${LIBCBCT_HEADERS})

target_include_directories(${LIBCBCT}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${PROJECT_SOURCE_DIR}/submodules/jsonhpp/include ${OpenCV_INCLUDE_DIRS})

target_compile_definitions(${LIBCBCT} PUBLIC $<$<CONFIG:Release>:LIBCBCT_DISABLE_ASSERT>)
if (MSVC)
  target_compile_definitions(${LIBCBCT} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(${LIBCBCT} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-format-security>)
endif()

# Only image I/O is needed by the library itself
target_link_libraries(${LIBCBCT} PRIVATE opencv_core opencv_imgcodecs)

if (LIBCBCT_WITH_OPENMP)
  target_link_libraries(${LIBCBCT} PUBLIC OpenMP::OpenMP_CXX)
endif()

if (LIBCBCT_WITH_CUDA)
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_WITH_CUDA)
  target_link_libraries(${LIBCBCT} PUBLIC CUDA::cudart CUDA::cufft PRIVATE CUDA::cuda_driver)
endif()

if (LIBCBCT_WITH_CUDA AND LIBCBCT_WITH_SLANG)
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_WITH_SLANG_CUDA)
  add_dependencies(${LIBCBCT} cbct_slang_ptx)
endif()

if (LIBCBCT_WITH_VULKAN)
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_WITH_VULKAN)
  target_link_libraries(${LIBCBCT} PRIVATE cbct_vma)
endif()

# ===============================================
# CBCT example executable
# ===============================================
add_executable(${LIBCBCT_EXE} main.cpp)

target_include_directories(${LIBCBCT_EXE} PRIVATE
  ${PROJECT_SOURCE_DIR}/submodules/cxxopts/include
  ${PROJECT_SOURCE_DIR}/submodules/jsonhpp/include
  ${OpenCV_INCLUDE_DIRS})

target_link_libraries(${LIBCBCT_EXE} PRIVATE ${LIBCBCT} opencv_core opencv_highgui)

if (LIBCBCT_WITH_CUDA AND LIBCBCT_WITH_SLANG)
  target_compile_definitions(${LIBCBCT_EXE} PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
endif()

if (MSVC)
  target_compile_definitions(${LIBCBCT_EXE} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  target_link_options(${LIBCBCT_EXE} PRIVATE /DEBUG /PROFILE /INCREMENTAL:NO)
endif()
