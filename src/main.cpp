#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <atomic>

#include "Common/OpenMP.h"

#include <opencv2/opencv.hpp>

int main(int argc, char **argv) {
    const int N = 1000;
    const float sod = 810.23710000;
    const float sdd = 1167.39735297;
    const float pixelSizeX = 0.2;
    const float pixelSizeY = 0.2;
    const float freeRay = 6000.0;

    printf("#project: %d\n", N);
    printf("SOD: %f\n", sod);
    printf("SDD: %f\n", sdd);

    std::vector<cv::Mat> sinogram(N);
    OMP_PARALLEL_FOR(int i = 0; i < N; i++) {
        char filename[256];
        sprintf(filename, "./data/amazon_cast/ScatterCorrect%05d.tif", i + 1);
        cv::Mat image = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            fprintf(stderr, "Failed to load image: %s\n", filename);
            std::exit(1);
        }

        cv::Mat temp;
        image.convertTo(temp, CV_32F, 1.0f / freeRay);
        cv::log(temp, temp);
        temp = -temp;

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

        printf("%s\n", filename);
        sinogram[i] = temp;
    }

    const int detWidth = sinogram[0].cols;
    const int detHeight = sinogram[0].rows;
    printf("detW: %d\n", detWidth);
    printf("detH: %d\n", detHeight);

    const int sizeX = 1000;
    const int sizeY = 1000;
    const int sizeZ = 1000;
    const float voxelSize = (detWidth * pixelSizeX) * sod / sdd / sizeX;

    std::vector<cv::Mat> res(sizeZ);

    OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
        res[z] =  cv::Mat::zeros(sizeY, sizeX, CV_32F);
    }

    std::atomic<float> maxVal(-(float)FLT_MAX);

    OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                for (int i = 0; i < N; i++) {                
                    const float theta = 2.0 * M_PI * i / N;
                    const float cosTheta = std::cos(theta);
                    const float sinTheta = std::sin(theta);
                    const float vx = sod + voxelSize * (x - sizeX / 2) * cosTheta - voxelSize * (y - sizeY / 2) * sinTheta;
                    const float vy = voxelSize * (x - sizeX / 2) * sinTheta + voxelSize * (y - sizeY / 2) * cosTheta;
                    const float vz = voxelSize * (z - sizeZ / 2);

                    const float u = vy * sdd / std::abs(vx);
                    const float v = vz * sdd / std::abs(vx);
                    const float iu = u / pixelSizeX + detWidth / 2;
                    const float iv = v / pixelSizeY + detHeight / 2;

                    if (iu >= 0.0 && iv >= 0.0 && iu < detWidth - 1 && iv < detHeight - 1) {
                        res[z].at<float>(y, x) += sinogram[i].at<float>(cv::Point2f(iu, iv));
                    }
                }
                res[z].at<float>(y, x) /= N;

                maxVal.store(std::max(maxVal.load(), res[z].at<float>(y, x)));
            }
        }
    }

    printf("maxVal = %f\n", maxVal.load());
    OMP_PARALLEL_FOR(int z = 0; z < sizeZ; z++) {
        res[z] /= maxVal;
    }

    std::ofstream writer("output.raw", std::ios::out | std::ios::binary);
    float *buffer = new float[sizeX];
    for (int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                buffer[x] = res[z].at<float>(y, x);
            }
            writer.write((char *)buffer, sizeof(float) * sizeX);
        }
    }
    writer.close();

    cv::imshow("res", res[sizeZ / 2]);
    cv::waitKey(0);
    cv::destroyAllWindows();
}
