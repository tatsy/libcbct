#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

namespace libcbct::testing_utils {

class TestDirectory {
public:
    TestDirectory() {
        static std::atomic<unsigned> sequence{ 0 };
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
               / ("libcbct-regression-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
        std::filesystem::create_directory(path);
    }
    ~TestDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    TestDirectory(const TestDirectory &) = delete;
    TestDirectory &operator=(const TestDirectory &) = delete;

    std::filesystem::path path;
};

}  // namespace libcbct::testing_utils
