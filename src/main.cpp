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
    config.read("../../data/amazon_cast/ScatterCorrect.json");

    const float sod = config.getFloat("sod");
    const float sdd = config.getFloat("sdd");
    const float pixelSizeX = config.getFloat("pixelSizeX");
    const float pixelSizeY = config.getFloat("pixelSizeY");
    const float freeRay = config.getFloat("freeRay");
    LIBCBCT_DEBUG("SOD: %f", sod);
    LIBCBCT_DEBUG("SDD: %f", sdd);
    LIBCBCT_DEBUG("Pixel size: (%f, %f)", pixelSizeX, pixelSizeY);
    LIBCBCT_DEBUG("Free ray: %f", freeRay);

    // Import sinogram
    FloatVolume sinogram;
    ImageSequenceImporter().read("../../data/amazon_cast/ScatterCorrect%05d.tif", sinogram);

    sinogram.forEach([freeRay](float v) -> float { return -std::log(v / freeRay); });

    const int detWidth = sinogram.size<0>();
    const int detHeight = sinogram.size<1>();
    const int N = sinogram.size<2>() - 1;
    LIBCBCT_DEBUG("Detector size: (%d, %d)", detWidth, detHeight);
    LIBCBCT_DEBUG("Sinogram: %dx%dx%d", detWidth, detHeight, N);

    // Setup projection geometry
    const int volSizeX = 256;
    const int volSizeY = 256;
    const int volSizeZ = 256;
    FloatVolume tomogram;
    Geometry geometry(vec2i(detWidth, detHeight), vec2f(pixelSizeX, pixelSizeY), vec3i(volSizeX, volSizeY, volSizeZ),
                      sod, sdd);

// Reconstruction
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
