#define LIBCBCT_API_EXPORT
#include "FeldkampCPU.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <opencv2/opencv.hpp>

#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"
#include "Utils/ImageUtils.h"

void FeldkampCPU::reconstruct(const VolumeF32 &sinogram, VolumeF32 &tomogram, const Geometry &geometry) const {
    const int detWidth = sinogram.size<0>();
    const int detHeight = sinogram.size<1>();
    const int nProj = sinogram.size<2>();

    const vec3i volSize = geometry.volSize;
    LIBCBCT_DEBUG("Volume size: %dx%dx%d", volSize.x, volSize.y, volSize.z);
    tomogram.resize(volSize.x, volSize.y, volSize.z);

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (int i = 0; i < nProj; i++) {
        float *const ptr = sinogram.ptr() + (detWidth * detHeight * (uint64_t)i);
        cv::Mat temp(detHeight, detWidth, CV_32FC1);
        std::memcpy(temp.data, ptr, sizeof(float) * detWidth * detHeight);

        cv::dft(temp, temp, cv::DFT_ROWS | cv::DFT_COMPLEX_OUTPUT);
        for (int y = 0; y < temp.rows; y++) {
            for (int x = 0; x < temp.cols; x++) {
                const float q = std::min(x, temp.cols - x) / (0.5f * temp.cols);
                if (filter == RampFilter::RamLak) {
                    temp.at<std::complex<float>>(y, x) *= std::abs(q);
                } else if (filter == RampFilter::SheppLogan) {
                    temp.at<std::complex<float>>(y, x) *= (2.0f / M_PI) * std::abs(std::sin(0.5f * M_PI * q));
                } else {
                    LIBCBCT_ERROR("Unknown ramp filter specified!");
                }
            }
        }
        cv::idft(temp, temp, cv::DFT_ROWS | cv::DFT_COMPLEX_INPUT | cv::DFT_REAL_OUTPUT);

        OMP_PARALLEL_FOR(int z = 0; z < volSize.z; z++) {
            for (int y = 0; y < volSize.y; y++) {
                for (int x = 0; x < volSize.x; x++) {
                    const float theta = 2.0f * (float)M_PI * i / nProj;
                    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
                    if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < detWidth && uvw.y < detHeight) {
                        tomogram(x, y, z) +=
                            bilerp((float *)temp.data, detWidth, detHeight, uvw.x - 0.5f, uvw.y - 0.5f) * uvw.z / nProj;
                    }
                }
            }
        }

        pbar.step();
    }
}
