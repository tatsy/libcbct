#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_BASE_EXPORTER_H
#define LIBCBCT_BASE_EXPORTER_H

#include <string>

#include "Common/Api.h"
#include "Utils/Volume.h"

/**
 * @brief Interface class for file exporter
 */
class LIBCBCT_API BaseExporter {
public:
    BaseExporter() = default;
    virtual ~BaseExporter() = default;

    virtual void write(const std::string &filename, FloatVolume &tomogram) const = 0;
};

#endif  // LIBCBCT_BASE_EXPORTER_H
