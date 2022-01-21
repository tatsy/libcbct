#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <atomic>
#include <mutex>

#include "Common/ProgressBar.h"
#include "Common/OpenMP.h"
#include "IO/ImageSequenceImporter.h"

#include <opencv2/opencv.hpp>

int main(int argc, char **argv) {
    const float sod = 810.23710000;
    const float sdd = 1167.39735297;
    const float pixelSizeX = 0.2;
    const float pixelSizeY = 0.2;
    const float freeRay = 6000.0;

    printf("SOD: %f\n", sod);
    printf("SDD: %f\n", sdd);

    FloatVolume sinogram;
    ImageSequenceImporter().read("../../data/amazon_cast/ScatterCorrect%05d.tif", sinogram);

    const int detWidth = sinogram.size<0>();
    const int detHeight = sinogram.size<1>();
    const int N = sinogram.size<2>() - 1;
    LIBCBCT_INFO("Sinogram: %dx%dx%d", detWidth, detHeight, N);

    LIBCBCT_INFO("Filtering...");
    ProgressBar pbar(N);
    OMP_PARALLEL_FOR(int i = 0; i < N; i++) {
        float *const ptr = sinogram.ptr() + (detWidth * detHeight * (uint64_t)i);
        cv::Mat temp(detHeight, detWidth, CV_32FC1);
        std::memcpy(temp.data, ptr, sizeof(float) * detWidth * detHeight);

        temp.convertTo(temp, CV_32F, 1.0 / freeRay);
        cv::log(temp, temp);
        temp.convertTo(temp, CV_32F, -1.0);

        cv::dft(temp, temp, cv::DFT_ROWS | cv::DFT_COMPLEX_OUTPUT);
        for (int y = 0; y < temp.rows; y++) {
            for (int x = 0; x < temp.cols; x++) {
                const float q = std::min(x, temp.cols - x) / (0.5f * temp.cols);
                // Ram-Lak
                // temp.at<std::complex<float>>(y, x) *= std::abs(q);
                // Shepp-Logan
                temp.at<std::complex<float>>(y, x) *= (2.0f / M_PI) * std::abs(std::sin(0.5f * M_PI * q));
            }
        }
        cv::idft(temp, temp, cv::DFT_ROWS | cv::DFT_COMPLEX_INPUT | cv::DFT_REAL_OUTPUT);
        std::memcpy(ptr, temp.data, sizeof(float) * detWidth * detHeight);
        pbar.step();
    }

    const int sizeX = 256;
    const int sizeY = 256;
    const int sizeZ = 256;
    const float voxelSize = (detWidth * pixelSizeX) * sod / sdd / sizeX;

    FloatVolume out(sizeX, sizeY, sizeZ);
    std::atomic<float> maxVal(-(float)FLT_MAX);

    LIBCBCT_INFO("Back-projecting...");
    pbar.reset(sizeZ);
    OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                for (int i = 0; i < N; i++) {
                    const float theta = 2.0 * M_PI * i / N;
                    const float cosTheta = std::cos(theta);
                    const float sinTheta = std::sin(theta);
                    const float vx =
                        sod + voxelSize * (x - sizeX / 2) * cosTheta - voxelSize * (y - sizeY / 2) * sinTheta;
                    const float vy = voxelSize * (x - sizeX / 2) * sinTheta + voxelSize * (y - sizeY / 2) * cosTheta;
                    const float vz = voxelSize * (z - sizeZ / 2);

                    const float u = vy * sdd / std::abs(vx);
                    const float v = vz * sdd / std::abs(vx);
                    const float iu = u / pixelSizeX + detWidth / 2;
                    const float iv = v / pixelSizeY + detHeight / 2;

                    if (iu >= 0.0 && iv >= 0.0 && iu < detWidth - 1 && iv < detHeight - 1) {
                        out(x, y, z) += sinogram(iu, iv, i);
                    }
                }
                out(x, y, z) /= N;

                maxVal.store(std::max(maxVal.load(), out(x, y, z)));
            }
        }
        pbar.step();
    }

    printf("maxVal = %f\n", maxVal.load());
    OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                out(x, y, z) /= maxVal;
            }
        }
    }

    std::ofstream writer("output.raw", std::ios::out | std::ios::binary);
    float *buffer = new float[sizeX];
    for (int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                buffer[x] = out(x, y, z);
            }
            writer.write((char *)buffer, sizeof(float) * sizeX);
        }
    }
    writer.close();

    cv::Mat slice(sizeY, sizeX, CV_32F, out.ptr() + (sizeX * sizeY * sizeZ / 2));
    cv::imshow("center slice", slice);
    cv::waitKey(0);
    cv::destroyAllWindows();
}
