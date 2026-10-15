#ifdef _MSC_VER
#pragma once
#endif

#ifndef SETTINGS_IMPORTER_BASE_H
#define SETTINGS_IMPORTER_BASE_H

#include <string>
#include <unordered_map>

#include "Common/Api.h"
#include "Common/Logging.h"
#include "Common/Exception.h"

namespace libcbct {

class LIBCBCT_API SettingsImporterBase {
public:
    SettingsImporterBase() = default;
    virtual ~SettingsImporterBase() = default;

    bool hasKey(const std::string &key) const {
        return this->data.count(key) != 0;
    }

    int getInt(const std::string &key, int defaultValue = 0) const {
        if (!this->hasKey(key)) {
            return defaultValue;
        }
        return (int)std::strtol(this->getString(key).c_str(), NULL, 10);
    }

    float getFloat(const std::string &key, float defaultValue = 0.0f) const {
        if (!this->hasKey(key)) {
            return defaultValue;
        }
        return std::strtod(this->getString(key).c_str(), NULL);
    }

    bool getBool(const std::string &key, bool defaultValue = false) const {
        if (!this->hasKey(key)) {
            return defaultValue;
        }
        const auto val = this->getString(key);
        char *end = nullptr;

        errno = 0;
        const double v = std::strtod(val.c_str(), &end);

        if (end == val.c_str() || end != val.c_str() + val.size() || errno == ERANGE || (v != 0.0 && v != 1.0)) {
            LIBCBCT_THROW<libcbct::InvalidArgument>("Invalid boolean value for key: %s", key.c_str());
        }

        return v == 1.0;
    }

    std::string getString(const std::string &key, const std::string &defaultValue = "") const {
        if (!this->hasKey(key)) {
            return defaultValue;
        }
        const auto it = this->data.find(key);
        return it->second;
    }

protected:
    std::unordered_map<std::string, std::string> data;
};

}  // namespace libcbct

#endif  // SETTINGS_IMPORTER_BASE_H
