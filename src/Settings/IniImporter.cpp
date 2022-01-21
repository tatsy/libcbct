#define LIBCBCT_API_EXPORT
#include "IniImporter.h"

#include <iostream>
#include <fstream>

#include "Common/Logging.h"

void IniImporter::read(const std::string &filename) {
    std::ifstream reader(filename, std::ios::in);
    if (reader.fail()) {
        LIBCBCT_ERROR("failed to open file: %s", filename.c_str());
    }
}
