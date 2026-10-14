#include "JsonSettingImporter.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <format>

#include <nlohmann/json.hpp>

#include "Common/Exception.h"
#include "Common/Logging.h"

using json = nlohmann::json;

void JsonSettingImporter::read(const std::string &filename) {
    std::ifstream reader(filename, std::ios::in);
    if (reader.fail()) {
        LIBCBCT_THROW<libcbct::IOError>("failed to open file: %s", filename.c_str());
    }

    // Parse JSON
    json js = json::parse(reader);
    for (auto it = js.begin(); it != js.end(); ++it) {
        if (it.value().is_string()) {
            data[it.key()] = it.value().get<std::string>();
        } else if (it.value().is_number()) {
            const auto val = it.value().get<float>();
            data[it.key()] = std::format("{:f}", val);
        }
    }

    reader.close();
}
