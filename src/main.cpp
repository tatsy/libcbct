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

static void onTrackbar(int pos, void *userdata) {
    const VolumeF32 &tomogram = *(VolumeF32 *)userdata;
    const int volSize = tomogram.size<0>();
    const cv::Mat slice(volSize, volSize, CV_32FC1, tomogram.ptr() + (volSize * volSize * pos));
    cv::imshow("volume", slice);
}

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
    LIBCBCT_DEBUG("OpenMP threads: %d", omp_get_max_threads());

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
        LIBCBCT_ERROR("Projection folder does not exist: %s", imagePath.string().c_str());
    }

    VolumeF32 sinogram = ImageSequenceImporter(imagePath.string(), ".tif", clockwise).read();
    sinogram.forEach([freeRay](float v) -> float { return -std::log((v + 1.0f) / freeRay); });

    LIBCBCT_ASSERT(
        sinogram.size<0>() == detWidth && sinogram.size<1>() == detHeight && sinogram.size<2>() - 1 == numberOfProj,
        "Sinogram size mismatch!");
    LIBCBCT_DEBUG("Detector size: (%d, %d)", detWidth, detHeight);
    LIBCBCT_DEBUG("Sinogram: %dx%dx%d", detWidth, detHeight, numberOfProj);

    // Setup projection geometry
    const int volSize = configs["size"].as<int>();
    const float voxelSize = pixelSizeY * (sod / sdd) * ((float)detHeight / (float)volSize);
    LIBCBCT_DEBUG("Reconstruction volume size: %d (%f mm/voxel)", volSize, voxelSize);

    Geometry geometry(vec2i(detWidth, detHeight), vec2f(pixelSizeX, pixelSizeY), vec3i(volSize, volSize, volSize), sod,
                      sdd);

// // Reconstruction
#if defined(LIBCBCT_WITH_CUDA)
    FeldkampCUDA fdk(RampFilter::SheppLogan);
    VolumeF32 tomogram = fdk.reconstruct(sinogram, geometry);
#else
    FeldkampCPU fdk(RampFilter::SheppLogan);
    VolumeF32 tomogram = fdk.reconstruct(sinogram, geometry);
#endif  // LIBCBCT_WITH_CUDA

    // Normalize CT values
    const auto [minVal, maxVal] = tomogram.getMinMax();
    LIBCBCT_DEBUG("min=%f, max=%f", minVal, maxVal);

    tomogram.forEach([minVal, maxVal](float x) { return (x - minVal) / (maxVal - minVal); });

    // Export tomogram
    const fs::path outputPath = configPath.parent_path() / "output" / "volume.raw";
    fs::create_directories(outputPath.parent_path());
    RawVolumeExporter exporter;
    exporter.write(outputPath.string(), tomogram);
    LIBCBCT_DEBUG("Reconstructed volume saved: %s", outputPath.string().c_str());

    // Preview
    cv::namedWindow("volume", cv::WINDOW_AUTOSIZE);
    cv::createTrackbar("#slice", "volume", nullptr, volSize - 1, onTrackbar, &tomogram);
    onTrackbar(volSize / 2, &tomogram);

    cv::waitKey(0);
    cv::destroyAllWindows();
}
