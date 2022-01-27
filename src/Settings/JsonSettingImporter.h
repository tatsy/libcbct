#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_JSON_SETTING_IMPORTER_H
#define LIBCBCT_JSON_SETTING_IMPORTER_H

#include "SettingsImporterBase.h"

/**
 * @brief Setting file importer with JSON format
 */
class LIBCBCT_API JsonSettingImporter : public SettingsImporterBase {
public:
    JsonSettingImporter() = default;
    virtual ~JsonSettingImporter() = default;

    void read(const std::string &filename);
};

#endif  // LIBCBCT_JSON_SETTING_IMPORTER_H
