#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <atomic>
#include <mutex>

#include <opencv2/opencv.hpp>

#include "libcbct.h"

int main(int argc, char **argv) {
    // Read device parameters
    JsonSettingImporter config;
    config.read("data/clip/config.json");

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
    Volume<float> sinogram(detWidth, detHeight, numberOfProj);
    ImageSequenceImporter(clockwise).read("data/clip/data_%06d.tif", sinogram);

    sinogram.forEach([freeRay](float v) -> float { return -std::log((v + 1.0f) / freeRay); });

    LIBCBCT_ASSERT(
        sinogram.size<0>() == detWidth && sinogram.size<1>() == detHeight && sinogram.size<2>() - 1 == numberOfProj,
        "Sinogram size mismatch!");
    LIBCBCT_DEBUG("Detector size: (%d, %d)", detWidth, detHeight);
    LIBCBCT_DEBUG("Sinogram: %dx%dx%d", detWidth, detHeight, numberOfProj);

    // Setup projection geometry
    const int volSizeX = 800;
    const int volSizeY = 800;
    const int volSizeZ = 800;
    FloatVolume tomogram;
    Geometry geometry(vec2i(detWidth, detHeight), vec2f(pixelSizeX, pixelSizeY), vec3i(volSizeX, volSizeY, volSizeZ),
                      sod, sdd);

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
    cv::Mat slice(volSizeY, volSizeX, CV_32F, tomogram.ptr() + (volSizeX * volSizeY * volSizeZ / 2));
    cv::imshow("center slice", slice);
    cv::waitKey(0);
    cv::destroyAllWindows();
}
