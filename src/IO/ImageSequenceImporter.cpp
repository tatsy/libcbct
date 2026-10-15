#include "ImageSequenceImporter.h"

#include <iostream>
#include <vector>
#include <atomic>
#include <filesystem>
#include <limits>

#include <opencv2/opencv.hpp>

#include "Common/Exception.h"
#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"

namespace libcbct {

namespace fs = std::filesystem;

VolumeF32 ImageSequenceImporter::read() const {
    if (projections && projections->numberOfProjections <= 0) {
        throw InvalidArgument("NumberOfProj must be positive");
    }
    // Get image file list
    std::vector<std::string> fileList;
    const fs::path folderPath(folder.c_str());
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        LIBCBCT_THROW<libcbct::IOError>("Invalid folder path: {}", folder);
    }

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
        LIBCBCT_THROW<libcbct::IOError>("No image files found in folder: {}", folder);
    }

    std::sort(fileList.begin(), fileList.end());

    if (projections) {
        const size_t effectiveCount = static_cast<size_t>(projections->numberOfProjections);
        const size_t expectedCount = effectiveCount + (projections->isEndpointDuplicate ? 1u : 0u);
        if (fileList.size() != expectedCount) {
            LIBCBCT_THROW<InvalidArgument>(
                "Projection count mismatch: NumberOfProj={}, IsEndpointDuplicate={}; expected {} files, found {}",
                projections->numberOfProjections, projections->isEndpointDuplicate, expectedCount, fileList.size());
        }
        // Discard the terminal angle-zero duplicate before reversing the acquisition direction.
        if (projections->isEndpointDuplicate) fileList.pop_back();
        // Keep the reference frame: [0, -d, -2d, ...] becomes [0, d, 2d, ...].
        if (projections->clockwise) std::reverse(fileList.begin() + 1, fileList.end());
    }
    if (fileList.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw InvalidArgument("Too many projection images");
    }

    // Get image size
    cv::Mat firstImage = cv::imread(fileList[0], cv::IMREAD_UNCHANGED);
    if (firstImage.empty()) {
        LIBCBCT_THROW<libcbct::IOError>("failed to open image: {}", fileList[0]);
    }
    const int width = firstImage.cols;
    const int height = firstImage.rows;
    const int nImages = static_cast<int>(fileList.size());

    if (firstImage.type() != CV_16UC1) {
        LIBCBCT_THROW<libcbct::IOError>("Unsupported image type: {}. Only 16-bit grayscale images are supported.",
                                        fileList[0]);
    }

    // Load images into sinogram volume
    VolumeF32 sinogram(width, height, nImages);

    std::atomic<int> failedIndex{ -1 };
    ProgressBar pbar(nImages);
    pbar.setDescription("IMPORT: ");
    OMP_PARALLEL_FOR(int i = 0; i < nImages; i++) {
        cv::Mat image;

        try {
            image = cv::imread(fileList[i], cv::IMREAD_UNCHANGED);
        } catch (const cv::Exception &e) {
            int expected = -1;
            failedIndex.compare_exchange_strong(expected, i);
            continue;
        }

        if (image.empty()) {
            int expected = -1;
            failedIndex.compare_exchange_strong(expected, i);
            continue;
        }

        if (image.type() != CV_16UC1) {
            int expected = -1;
            failedIndex.compare_exchange_strong(expected, i);
            continue;
        }

        if (image.cols != width || image.rows != height) {
            int expected = -1;
            failedIndex.compare_exchange_strong(expected, i);
            continue;
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const int index = reverseOrder ? (nImages - 1 - i) : i;
                sinogram(x, y, index) = (float)image.at<uint16_t>(y, x);
            }
        }
        pbar.step();
    }

    if (failedIndex >= 0) {
        LIBCBCT_THROW<libcbct::IOError>("failed to open image: {}", fileList[failedIndex]);
    }

    return sinogram;
}

}  // namespace libcbct
