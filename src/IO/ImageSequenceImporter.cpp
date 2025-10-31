#define LIBCBCT_API_EXPORT
#include "ImageSequenceImporter.h"

#include <opencv2/opencv.hpp>

#include "Common/Logging.h"
#include "Common/Path.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"

void ImageSequenceImporter::read(const std::string &format, FloatVolume &sinogram) const {
    int startIndex = 0;
    int nImages = 0;
    int width = 0;
    int height = 0;
    for (int i = 0;; i++) {
        char filename[256];
        sprintf(filename, format.c_str(), i);
        filepath path(filename);
        if (!path.exists()) {
            if (i == 0) {
                startIndex++;
                continue;
            }
            break;
        }
        nImages++;

        if (width == 0 && height == 0) {
            cv::Mat image = cv::imread(filename, cv::IMREAD_UNCHANGED);
            width = image.cols;
            height = image.rows;
        }
    }

    sinogram = FloatVolume(width, height, nImages);
    ProgressBar pbar(nImages);
    pbar.setDescription("IMPORT: ");
    OMP_PARALLEL_FOR(int i = 0; i < nImages; i++) {
        char filename[256];
        sprintf(filename, format.c_str(), startIndex + i);

        cv::Mat image = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            LIBCBCT_ERROR("failed to open image: %s", filename);
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int index = reverseOrder ? (nImages - 1 - i) : i;
                sinogram(x, y, index) = (float)image.at<uint16_t>(y, x);
            }
        }
        pbar.step();
    }
}
