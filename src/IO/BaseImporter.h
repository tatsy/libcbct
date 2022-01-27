#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_BASE_IMPORTER_H
#define LIBCBCT_BASE_IMPORTER_H

#include <string>

#include "Common/Api.h"
#include "Utils/Volume.h"

/**
 * @brief Interface class for file importer
 */
class LIBCBCT_API BaseImporter {
public:
    BaseImporter() = default;
    virtual ~BaseImporter() = default;

    virtual void read(const std::string &filename, FloatVolume &sinogram) const = 0;
};

#endif  // LIBCBCT_BASE_IMPORTER_H
