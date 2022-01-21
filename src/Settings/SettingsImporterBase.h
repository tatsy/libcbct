#ifdef _MSC_VER
#pragma once
#endif

#ifndef SETTINGS_IMPORTER_BASE_H
#define SETTINGS_IMPORTER_BASE_H

#include <string>
#include <unordered_map>

#include "Common/Api.h"

class SettingsImporterBase {
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

    std::string getString(const std::string &key, const std::string &defaultValue = "") const {
        if (!this->hasKey(key)) {
            return defaultValue;
        }
        const auto it = this->data.find(key);
        return it->second;
    }

private:
    std::unordered_map<std::string, std::string> data;
};

#endif  // SETTINGS_IMPORTER_BASE_H
