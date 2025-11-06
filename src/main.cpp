#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <atomic>
#include <mutex>
#include <filesystem>

#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "libcbct.h"

namespace fs = std::filesystem;

int main(int argc, char **argv) {
    // Parse command line options
    cxxopts::Options options("cbct_ext");
    options.add_options()("h,help", "Print help");
    options.add_options()("c,config", "Input configuration file", cxxopts::value<std::string>());
    options.add_options()("s,size", "Reconstruction volume size (cubic)", cxxopts::value<int>()->default_value("512"));
    const auto configs = options.parse(argc, argv);

    if (configs["config"].count() == 0) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // Read device parameters
    fs::path configPath(configs["config"].as<std::string>());
    JsonSettingImporter config;
    config.read(configPath.string());

    const float sod = config.getFloat("SOD");
    const float sdd = config.getFloat("SDD");
    const float pixelSizeX = config.getFloat("PixelSizeX");
    const float pixelSizeY = config.getFloat("PixelSizeY");
    const int detWidth = config.getInt("DetWidth");
    const int detHeight = config.getInt("DetHeight");
    const int numberOfProj = config.getInt("NumberOfProj");
    const bool clockwise = config.getInt("Clockwise", 1) != 0;
    const float freeRay = 65535.0f;  // Assuming 16-bit images
    LIBCBCT_DEBUG("SOD: %f", sod);
    LIBCBCT_DEBUG("SDD: %f", sdd);
    LIBCBCT_DEBUG("Pixel size: (%f, %f)", pixelSizeX, pixelSizeY);

    // Import sinogram
    const fs::path imagePath = configPath.parent_path();
    VolumeF32 sinogram = ImageSequenceImporter(imagePath.string(), ".tif", clockwise).read();
    sinogram.forEach([freeRay](float v) -> float { return -std::log((v + 1.0f) / freeRay); });

    LIBCBCT_ASSERT(
        sinogram.size<0>() == detWidth && sinogram.size<1>() == detHeight && sinogram.size<2>() - 1 == numberOfProj,
        "Sinogram size mismatch!");
    LIBCBCT_DEBUG("Detector size: (%d, %d)", detWidth, detHeight);
    LIBCBCT_DEBUG("Sinogram: %dx%dx%d", detWidth, detHeight, numberOfProj);

    // Setup projection geometry
    const int volSize = configs["size"].as<int>();
    LIBCBCT_DEBUG("Reconstruction volume size: %d", volSize);

    VolumeF32 tomogram;
    Geometry geometry(vec2i(detWidth, detHeight), vec2f(pixelSizeX, pixelSizeY), vec3i(volSize, volSize, volSize), sod,
                      sdd);

// // Reconstruction
#if defined(LIBCBCT_WITH_CUDA)
    FeldkampCUDA fdk(RampFilter::SheppLogan);
    fdk.reconstruct(sinogram, tomogram, geometry);
#else
    FeldkampCPU fdk(RampFilter::SheppLogan);
    fdk.reconstruct(sinogram, tomogram, geometry);
#endif  // LIBCBCT_WITH_CUDA

    // Normalize CT values
    const float maxVal = tomogram.reduce([](float a, float b) -> float { return std::max(a, b); }, -(float)FLT_MAX);
    LIBCBCT_DEBUG("Max value: %f", maxVal);

    tomogram.forEach([maxVal](float x) -> float { return x / maxVal; });

    // Export tomogram
    RawVolumeExporter exporter;
    exporter.write("output.raw", tomogram);

    // Preview center slice
    cv::Mat slice(volSize, volSize, CV_32F, tomogram.ptr() + (volSize * volSize * volSize / 2));
    cv::imshow("center slice", slice);
    cv::waitKey(0);
    cv::destroyAllWindows();
}
