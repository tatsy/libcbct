#include "JsonSettingImporter.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <format>

#include <nlohmann/json.hpp>

#include "Common/Exception.h"
#include "Common/Logging.h"

namespace libcbct {

using json = nlohmann::json;

void JsonSettingImporter::read(const std::string &filename) {
    std::ifstream reader(filename, std::ios::in);
    if (reader.fail()) {
        LIBCBCT_THROW<libcbct::IOError>("failed to open file: {}", filename);
    }

    // Parse JSON
    json js = json::parse(reader);
    std::unordered_map<std::string, std::string> buf;
    for (auto it = js.begin(); it != js.end(); ++it) {
        if (it.value().is_string()) {
            buf[it.key()] = it.value().get<std::string>();
        } else if (it.value().is_number()) {
            buf[it.key()] = it.value().dump();
        } else if (it.value().is_boolean()) {
            const auto val = it.value().get<bool>();
            buf[it.key()] = val ? "1" : "0";
        } else {
            LIBCBCT_THROW<libcbct::InvalidArgument>("Unsupported value type for key: {}", it.key());
        }
    }

    data = std::move(buf);
    reader.close();
}

}  // namespace libcbct
