#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <memory>

#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "libcbct.h"

#if defined(LIBCBCT_WITH_VULKAN)
#include "Utils/VulkanCompute.h"
#endif

namespace fs = std::filesystem;
namespace cbct = libcbct;

namespace {

static void onTrackbar(int pos, void *userdata) {
    cbct::VolumeF32 &tomogram = *(cbct::VolumeF32 *)userdata;
    const size_t volSize = tomogram.size<0>();
    const cv::Mat slice(volSize, volSize, CV_32FC1, tomogram.ptr() + (volSize * volSize * pos));
    cv::imshow("volume", slice);
}

int run(int argc, char **argv) {
    // Parse command line options
    cxxopts::Options options("cbct_ext");
    options.add_options()("h,help", "Print help");
    options.add_options()("c,config", "Input configuration file", cxxopts::value<std::string>());
    options.add_options()("s,size", "Reconstruction volume size (cubic)", cxxopts::value<int>()->default_value("512"));
    options.add_options()("b,backend", "Reconstruction backend: auto, cpu, cuda, vulkan",
                          cxxopts::value<std::string>()->default_value("auto"));
#if defined(LIBCBCT_WITH_CUDA)
    const std::string defaultPtxDirectory = LIBCBCT_SLANG_KERNEL_DIR;
#else
    const std::string defaultPtxDirectory;
#endif
    options.add_options()("ptx-dir", "Directory containing FDK PTX and reflection files",
                          cxxopts::value<std::string>()->default_value(defaultPtxDirectory));
#if defined(LIBCBCT_WITH_VULKAN) && defined(LIBCBCT_SLANG_KERNEL_DIR)
    const std::string defaultSpirvDirectory = LIBCBCT_SLANG_KERNEL_DIR;
#else
    const std::string defaultSpirvDirectory;
#endif
    options.add_options()("spirv-dir", "Directory containing FDK/FFT SPIR-V and reflection files",
                          cxxopts::value<std::string>()->default_value(defaultSpirvDirectory));
    options.add_options()("vulkan-device", "Vulkan physical device index (-1 selects automatically)",
                          cxxopts::value<int>()->default_value("-1"));
    options.add_options()("no-preview", "Save the reconstructed volume without opening a preview window");
    const auto configs = options.parse(argc, argv);

    if (configs.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::string backend = configs["backend"].as<std::string>();

    // Auto-select backend if not specified
    if (backend == "auto") {
        backend = "cpu";

        // Check for CUDA availability
#if defined(LIBCBCT_WITH_CUDA)
        int deviceCount = 0;
        if (cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0) backend = "cuda";
#endif

        // Check for Vulkan availability
#if defined(LIBCBCT_WITH_VULKAN)
        if (backend == "cpu") {
            try {
                if (!cbct::VulkanComputeContext::availableDevices().empty()) backend = "vulkan";
            } catch (const cbct::VulkanUnavailable &) {
                // A Vulkan build can run on a host without a compatible driver.
            }
        }
#endif
    }

    // Create reconstruction object based on the selected backend
    std::unique_ptr<cbct::ReconstructionBase> fdk;
    if (backend == "cpu") {
        fdk = std::make_unique<cbct::FeldkampCpu>(cbct::RampFilter::SheppLogan);
    } else if (backend == "cuda") {
#if defined(LIBCBCT_WITH_CUDA)
        fdk = std::make_unique<cbct::FeldkampCuda>(configs["ptx-dir"].as<std::string>(), cbct::RampFilter::SheppLogan);
#else
        throw libcbct::InvalidArgument("CUDA backend is unavailable; rebuild with LIBCBCT_WITH_CUDA=ON");
#endif
    } else if (backend == "vulkan") {
#if defined(LIBCBCT_WITH_VULKAN)
        fdk = std::make_unique<cbct::FeldkampVulkan>(configs["spirv-dir"].as<std::string>(),
                                                     cbct::RampFilter::SheppLogan, configs["vulkan-device"].as<int>());
#else
        throw libcbct::InvalidArgument("Vulkan backend is unavailable; rebuild with LIBCBCT_WITH_VULKAN=ON");
#endif
    } else {
        throw libcbct::InvalidArgument("Unknown backend '" + backend + "'; choose auto, cpu, cuda, or vulkan");
    }

    const int volSize = configs["size"].as<int>();
    if (volSize <= 0) throw libcbct::InvalidArgument("Reconstruction size must be positive");
    if (configs["config"].count() == 0) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    cbct::LIBCBCT_INFO("Reconstruction backend: {}", backend);
    cbct::LIBCBCT_INFO("OpenMP threads: {}", omp_get_max_threads());
    if (backend == "cuda") cbct::showCudaInfo();

    // Read device parameters
    fs::path configPath(configs["config"].as<std::string>());
    cbct::JsonSettingImporter config;
    config.read(configPath.string());
    cbct::LIBCBCT_DEBUG("Load config file from {}", configPath.string());

    const float sod = config.getFloat("SOD");
    const float sdd = config.getFloat("SDD");
    const float pixelSizeX = config.getFloat("PixelSizeX");
    const float pixelSizeY = config.getFloat("PixelSizeY");
    const int detWidth = config.getInt("DetWidth");
    const int detHeight = config.getInt("DetHeight");
    const int numberOfProj = config.getInt("NumberOfProj");
    const bool clockwise = config.getBool("Clockwise", true);
    const bool isEndpointDuplicate = config.getBool("EndpointDuplicate", false);
    const float freeRay = 65535.0f;  // Assuming 16-bit images
    cbct::LIBCBCT_DEBUG("SOD: {:f} mm", sod);
    cbct::LIBCBCT_DEBUG("SDD: {:f} mm", sdd);
    cbct::LIBCBCT_DEBUG("Pixel size: ({:f} mm, {:f} mm)", pixelSizeX, pixelSizeY);

    // Import sinogram
    const fs::path imagePath = configPath.parent_path() / "projections";
    if (!fs::exists(imagePath)) {
        cbct::LIBCBCT_THROW<libcbct::IOError>("Projection folder does not exist: {}", imagePath.string());
    }

    const cbct::ProjectionSequenceOptions projectionOptions{ numberOfProj, clockwise, isEndpointDuplicate };
    cbct::VolumeF32 sinogram = cbct::ImageSequenceImporter(imagePath.string(), ".tif", projectionOptions).read();

    if (sinogram.size<0>() != detWidth || sinogram.size<1>() != detHeight || sinogram.size<2>() != numberOfProj) {
        cbct::LIBCBCT_ERROR("Sinogram size mismatch! Expected: ({}, {}, {}), Actual: ({}, {}, {})", detWidth, detHeight,
                            numberOfProj, sinogram.size<0>(), sinogram.size<1>(), sinogram.size<2>());
    }
    sinogram.forEach([freeRay](float v) -> float { return -std::log((v + 1.0f) / freeRay); });

    cbct::LIBCBCT_DEBUG("Detector size: ({}, {})", detWidth, detHeight);
    cbct::LIBCBCT_DEBUG("Sinogram: {}x{}x{}", detWidth, detHeight, numberOfProj);

    // Setup projection geometry
    const float voxelSize = pixelSizeY * (sod / sdd) * ((float)detHeight / (float)volSize);
    cbct::LIBCBCT_DEBUG("Reconstruction volume size: {} ({:f} mm/voxel)", volSize, voxelSize);

    cbct::Geometry geometry(cbct::vec2i(detWidth, detHeight), cbct::vec2f(pixelSizeX, pixelSizeY),
                            cbct::vec3i(volSize, volSize, volSize), sod, sdd);

    // Reconstruction
    cbct::VolumeF32 tomogram = fdk->reconstruct(sinogram, geometry);

    // Normalize CT values
    float minVal, maxVal;
    std::tie(minVal, maxVal) = tomogram.getMinMax();
    cbct::LIBCBCT_DEBUG("min={:f}, max={:f}", minVal, maxVal);

    cbct::normalizePositiveMaximum(tomogram);

    // Export tomogram
    const fs::path outputPath =
        configPath.parent_path() / "output" / std::format("volume-{0:d}x{0:d}x{0:d}-uint16.raw", volSize);
    fs::create_directories(outputPath.parent_path());

    cbct::RawVolumeExporter exporter;
    exporter.write(outputPath.string(), tomogram, cbct::VolumeType::Uint16);
    cbct::LIBCBCT_DEBUG("Reconstructed volume saved: {}", outputPath.string());

    // Preview
    if (configs.count("no-preview")) return 0;

    int initPos = volSize / 2;
    cv::namedWindow("volume", cv::WINDOW_AUTOSIZE);
    cv::createTrackbar("#slice", "volume", &initPos, volSize - 1, onTrackbar, &tomogram);
    onTrackbar(initPos, &tomogram);

    cv::waitKey(0);
    cv::destroyAllWindows();

    return 0;
}

}  // anonymous namespace

int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const libcbct::Error &e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[ERROR] Unknown error occurred." << std::endl;
        return 1;
    }
}
