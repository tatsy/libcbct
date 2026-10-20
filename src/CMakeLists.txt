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
  Reconstruction/BufferStorage.h
  Reconstruction/CpuBufferStorage.h
  Reconstruction/FeldkampCpu.h
  Reconstruction/Feldkamp.h
  Reconstruction/Execution.h
  Reconstruction/ExecutionContext.h
  Reconstruction/Operators.h
  Reconstruction/CpuOperators.h
  Reconstruction/detail/FdkRecipe.h
  Reconstruction/detail/FdkValidation.h
  Reconstruction/FilterUtils.h
  Reconstruction/ReconstructionBase.h
  Reconstruction/pocketfft_hdronly.h
  Settings/JsonSettingImporter.h
  Settings/SettingsImporterBase.h
  Utils/CheckedSize.h
  Utils/CudaUtils.h
  Utils/ImageUtils.h
  Utils/Normalization.h
  Utils/ParseNumber.h
  Utils/Vec.h
  Utils/Volume.h
)

set(LIBCBCT_SOURCES
  IO/ImageSequenceImporter.cpp
  IO/RawVolumeExporter.cpp
  Reconstruction/FeldkampCpu.cpp
  Reconstruction/Feldkamp.cpp
  Reconstruction/CpuOperators.cpp
  Reconstruction/FilterUtils.cpp
  Settings/JsonSettingImporter.cpp
)

if (LIBCBCT_WITH_CUDA)
  list(APPEND LIBCBCT_HEADERS
    Reconstruction/CudaOperators.h
    Reconstruction/FeldkampCuda.h
    Reconstruction/Slang/FdkCudaParameters.h
    Utils/DeviceBuffer.h
    Utils/CudaPtxModule.h)
  list(APPEND LIBCBCT_SOURCES
    Reconstruction/CudaOperators.cpp
    Reconstruction/FeldkampCuda.cpp
    Utils/CudaPtxModule.cpp)
endif()

if (LIBCBCT_WITH_VULKAN)
  list(APPEND LIBCBCT_HEADERS
    Utils/VulkanCompute.h
    Utils/SlangSpirvKernel.h
    Utils/VulkanFft.h
    Reconstruction/FeldkampVulkan.h)
  list(APPEND LIBCBCT_SOURCES
    Utils/VulkanCompute.cpp
    Utils/SlangSpirvKernel.cpp
    Utils/VulkanFft.cpp
    Reconstruction/FeldkampVulkan.cpp)

  # Compile VMA in one translation unit, reusable by the library and Vulkan tests.
  add_library(cbct_vma STATIC Utils/VulkanMemoryAllocator.cpp)
  target_link_libraries(cbct_vma PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
  # The compute context supplies its loader's proc-address functions to VMA.
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

target_compile_features(${LIBCBCT} PUBLIC cxx_std_20)

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
  # Keep the former feature macro for source compatibility.
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_WITH_SLANG_CUDA)
  target_compile_definitions(${LIBCBCT} PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
  add_dependencies(${LIBCBCT} cbct_slang_ptx)
endif()

if (LIBCBCT_WITH_VULKAN)
  target_compile_definitions(${LIBCBCT} PUBLIC LIBCBCT_WITH_VULKAN)
  target_link_libraries(${LIBCBCT} PRIVATE cbct_vma)
  add_dependencies(${LIBCBCT} cbct_slang_vulkan_kernels)
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

if (LIBCBCT_WITH_SLANG)
  target_compile_definitions(${LIBCBCT_EXE} PRIVATE
    LIBCBCT_SLANG_KERNEL_DIR="${LIBCBCT_SLANG_KERNEL_OUTPUT_DIR}")
endif()

if (MSVC)
  target_compile_definitions(${LIBCBCT_EXE} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  target_link_options(${LIBCBCT_EXE} PRIVATE /DEBUG /PROFILE /INCREMENTAL:NO)
endif()
