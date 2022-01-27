#define LIBCBCT_API_EXPORT
#include "JsonSettingImporter.h"

#include <cstdio>
#include <iostream>
#include <fstream>

#include "Common/json.hpp"
#include "Common/Logging.h"

using json = nlohmann::json;

void JsonSettingImporter::read(const std::string &filename) {
    std::ifstream reader(filename, std::ios::in);
    if (reader.fail()) {
        LIBCBCT_ERROR("failed to open file: %s", filename.c_str());
    }

    // Read all the contents
    std::string content;
    reader.seekg(0, std::ios::end);
    content.reserve(reader.tellg());
    reader.seekg(0, std::ios::beg);
    content.assign(std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>());
    reader.close();

    // Parse JSON
    json js = json::parse(content);
    for (auto it = js.begin(); it != js.end(); ++it) {
        if (it.value().is_string()) {
            data[it.key()] = it.value().get<std::string>();
        } else if (it.value().is_number()) {
            char buf[256];
            sprintf(buf, "%f\n", it.value().get<float>());
            data[it.key()] = std::string(buf);
        }
    }
}
