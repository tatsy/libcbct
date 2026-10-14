# Vulkan's loader and development headers are supplied by the user or Vulkan SDK.
find_package(Vulkan REQUIRED)
if (NOT EXISTS "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.hpp")
  message(FATAL_ERROR
    "Vulkan-Hpp (vulkan/vulkan.hpp) was not found in ${Vulkan_INCLUDE_DIR}. "
    "Install the Vulkan SDK or a Vulkan development package containing Vulkan-Hpp.")
endif()

function(libcbct_fetch_vma)
  # Keep dependency options local to avoid affecting the parent project's settings.
  set(VMA_ENABLE_INSTALL OFF)
  set(VMA_BUILD_SAMPLES OFF)
  set(VMA_BUILD_DOCUMENTATION OFF)
  FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(vma)
endfunction()

libcbct_fetch_vma()
