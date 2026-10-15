#define _USE_MATH_DEFINES
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

namespace fs = std::filesystem;

static void onTrackbar(int pos, void *userdata) {
    const VolumeF32 &tomogram = *(VolumeF32 *)userdata;
    const int volSize = tomogram.size<0>();
    const cv::Mat slice(volSize, volSize, CV_32FC1, tomogram.ptr() + (volSize * volSize * pos));
    cv::imshow("volume", slice);
}

int run(int argc, char **argv) {
    // Parse command line options
    cxxopts::Options options("cbct_ext");
    options.add_options()("h,help", "Print help");
    options.add_options()("c,config", "Input configuration file", cxxopts::value<std::string>());
    options.add_options()("s,size", "Reconstruction volume size (cubic)", cxxopts::value<int>()->default_value("512"));
    options.add_options()("b,backend", "Reconstruction backend: auto, cpu, cuda, slang-ptx, vulkan",
                          cxxopts::value<std::string>()->default_value("auto"));
#if defined(LIBCBCT_WITH_SLANG_CUDA)
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
    if (backend == "auto") {
        backend = "cpu";
#if defined(LIBCBCT_WITH_CUDA)
        int deviceCount = 0;
        if (cudaGetDeviceCount(&deviceCount) == cudaSuccess && deviceCount > 0) backend = "cuda";
#endif
    }
    std::unique_ptr<ReconstructionBase> fdk;
    if (backend == "cpu") {
        fdk = std::make_unique<FeldkampCPU>(RampFilter::SheppLogan);
    } else if (backend == "cuda") {
#if defined(LIBCBCT_WITH_CUDA)
        fdk = std::make_unique<FeldkampCUDA>(RampFilter::SheppLogan);
#else
        throw libcbct::InvalidArgument("CUDA backend is unavailable; rebuild with LIBCBCT_WITH_CUDA=ON");
#endif
    } else if (backend == "slang-ptx") {
#if defined(LIBCBCT_WITH_SLANG_CUDA)
        fdk = std::make_unique<FeldkampSlangCUDA>(configs["ptx-dir"].as<std::string>(), RampFilter::SheppLogan);
#else
        throw libcbct::InvalidArgument(
            "Slang PTX backend is unavailable; rebuild with LIBCBCT_WITH_CUDA=ON and LIBCBCT_WITH_SLANG=ON");
#endif
    } else if (backend == "vulkan") {
#if defined(LIBCBCT_WITH_VULKAN)
        fdk = std::make_unique<FeldkampVulkan>(configs["spirv-dir"].as<std::string>(),
            RampFilter::SheppLogan, configs["vulkan-device"].as<int>());
#else
        throw libcbct::InvalidArgument("Vulkan backend is unavailable; rebuild with LIBCBCT_WITH_VULKAN=ON");
#endif
    } else {
        throw libcbct::InvalidArgument("Unknown backend '" + backend + "'; choose auto, cpu, cuda, slang-ptx, or vulkan");
    }
    const int volSize = configs["size"].as<int>();
    if (volSize <= 0) throw libcbct::InvalidArgument("Reconstruction size must be positive");
    if (configs["config"].count() == 0) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    LIBCBCT_INFO("Reconstruction backend: %s", backend.c_str());
    LIBCBCT_INFO("OpenMP threads: %d", omp_get_max_threads());
    if (backend == "cuda" || backend == "slang-ptx") showCudaInfo();

    // Read device parameters
    fs::path configPath(configs["config"].as<std::string>());
    JsonSettingImporter config;
    config.read(configPath.string());
    LIBCBCT_DEBUG("Load config file from %s", configPath.string().c_str());

    const float sod = config.getFloat("SOD");
    const float sdd = config.getFloat("SDD");
    const float pixelSizeX = config.getFloat("PixelSizeX");
    const float pixelSizeY = config.getFloat("PixelSizeY");
    const int detWidth = config.getInt("DetWidth");
    const int detHeight = config.getInt("DetHeight");
    const int numberOfProj = config.getInt("NumberOfProj");
    const bool clockwise = config.getInt("Clockwise", 1) != 0;
    const float freeRay = 65535.0f;  // Assuming 16-bit images
    LIBCBCT_DEBUG("SOD: %f mm", sod);
    LIBCBCT_DEBUG("SDD: %f mm", sdd);
    LIBCBCT_DEBUG("Pixel size: (%f mm, %f mm)", pixelSizeX, pixelSizeY);

    // Import sinogram
    const fs::path imagePath = configPath.parent_path() / "projections";
    if (!fs::exists(imagePath)) {
        LIBCBCT_THROW<libcbct::IOError>("Projection folder does not exist: %s", imagePath.string().c_str());
    }

    VolumeF32 sinogram = ImageSequenceImporter(imagePath.string(), ".tif", clockwise).read();
    sinogram.forEach([freeRay](float v) -> float { return -std::log((v + 1.0f) / freeRay); });

    if (sinogram.size<0>() != detWidth || sinogram.size<1>() != detHeight || sinogram.size<2>() - 1 != numberOfProj) {
        LIBCBCT_ERROR("Sinogram size mismatch! Expected: (%d, %d, %d), Actual: (%zu, %zu, %zu)", detWidth, detHeight,
                      numberOfProj, sinogram.size<0>(), sinogram.size<1>(), sinogram.size<2>());
    }

    LIBCBCT_DEBUG("Detector size: (%d, %d)", detWidth, detHeight);
    LIBCBCT_DEBUG("Sinogram: %dx%dx%d", detWidth, detHeight, numberOfProj);

    // Setup projection geometry
    const float voxelSize = pixelSizeY * (sod / sdd) * ((float)detHeight / (float)volSize);
    LIBCBCT_DEBUG("Reconstruction volume size: %d (%f mm/voxel)", volSize, voxelSize);

    Geometry geometry(vec2i(detWidth, detHeight), vec2f(pixelSizeX, pixelSizeY), vec3i(volSize, volSize, volSize), sod,
                      sdd);

    // Reconstruction
    VolumeF32 tomogram = fdk->reconstruct(sinogram, geometry);

    // Normalize CT values
    float minVal, maxVal;
    std::tie(minVal, maxVal) = tomogram.getMinMax();
    LIBCBCT_DEBUG("min=%f, max=%f", minVal, maxVal);

    tomogram.forEach([minVal, maxVal](float x) { return std::max(0.0f, x) / maxVal; });

    // Export tomogram
    const fs::path outputPath =
        configPath.parent_path() / "output" / std::format("volume-{0:d}x{0:d}x{0:d}-uint16.raw", volSize);
    fs::create_directories(outputPath.parent_path());

    RawVolumeExporter exporter;
    exporter.write(outputPath.string(), tomogram, VolumeType::Uint16);
    LIBCBCT_DEBUG("Reconstructed volume saved: %s", outputPath.string().c_str());

    // Preview
    if (configs.count("no-preview")) return 0;
    cv::namedWindow("volume", cv::WINDOW_AUTOSIZE);
    cv::createTrackbar("#slice", "volume", nullptr, volSize - 1, onTrackbar, &tomogram);
    onTrackbar(volSize / 2, &tomogram);

    cv::waitKey(0);
    cv::destroyAllWindows();

    return 0;
}

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
