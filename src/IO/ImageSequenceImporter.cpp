#define LIBCBCT_API_EXPORT
#include "ImageSequenceImporter.h"

#include <iostream>
#include <vector>
#include <filesystem>
#include <opencv2/opencv.hpp>

#include "Common/Logging.h"
#include "Common/Path.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"

namespace fs = std::filesystem;

VolumeF32 ImageSequenceImporter::read() const {
    // Get image file list
    std::vector<std::string> fileList;
    const fs::path folderPath(folder.c_str());
    for (const auto &entry : fs::directory_iterator(folderPath)) {
        if (fs::is_directory(entry.path())) {
            continue;
        }

        const std::string ext = entry.path().extension().string();
        if (ext == extension) {
            fileList.push_back(entry.path().string());
        }
    }

    if (fileList.empty()) {
        LIBCBCT_ERROR("No image files found in folder: %s", folder.c_str());
    }

    std::sort(fileList.begin(), fileList.end());

    // Get image size
    cv::Mat firstImage = cv::imread(fileList[0], cv::IMREAD_UNCHANGED);
    if (firstImage.empty()) {
        LIBCBCT_ERROR("failed to open image: %s", fileList[0].c_str());
    }
    const int width = firstImage.cols;
    const int height = firstImage.rows;
    const int nImages = static_cast<int>(fileList.size());

    // Load images into sinogram volume
    VolumeF32 sinogram(width, height, nImages);

    ProgressBar pbar(nImages);
    pbar.setDescription("IMPORT: ");
    OMP_PARALLEL_FOR(int i = 0; i < nImages; i++) {
        cv::Mat image = cv::imread(fileList[i], cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            LIBCBCT_ERROR("failed to open image: %s", fileList[i].c_str());
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int index = reverseOrder ? (nImages - 1 - i) : i;
                sinogram(x, y, index) = (float)image.at<uint16_t>(y, x);
            }
        }
        pbar.step();
    }

    return sinogram;
}
