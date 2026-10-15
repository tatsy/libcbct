#include <gtest/gtest.h>

#include <filesystem>

#include "Common/Exception.h"
#include "IO/ImageSequenceImporter.h"
#include "Settings/JsonSettingImporter.h"

namespace libcbct::tests {

namespace fs = std::filesystem;

TEST(Errors, MissingProjectionFolderThrowsIOError) {
    const ImageSequenceImporter importer("this/folder/does/not/exist", ".tif");
    EXPECT_THROW(importer.read(), libcbct::IOError);
}

TEST(Errors, EmptyProjectionFolderThrowsIOError) {
    const fs::path dir = fs::temp_directory_path() / "libcbct_test_empty_folder";
    fs::create_directories(dir);
    const ImageSequenceImporter importer(dir.string(), ".tif");
    EXPECT_THROW(importer.read(), libcbct::IOError);
    fs::remove_all(dir);
}

TEST(Errors, MissingConfigFileThrowsIOError) {
    JsonSettingImporter config;
    EXPECT_THROW(config.read("this/file/does/not/exist.json"), libcbct::IOError);
}

}  // namespace libcbct::tests
