#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_INI_IMPORTER_H
#define LIBCBCT_INI_IMPORTER_H

#include "Common/Api.h"
#include "SettingsImporterBase.h"

/**
 * @brief Setting file importer for *.ini format
 */
class LIBCBCT_API IniImporter : public SettingsImporterBase {
public:
    IniImporter() = default;
    virtual ~IniImporter() = default;

    void read(const std::string &filename);
};

#endif  // LIBCBCT_INI_IMPORTER_H
