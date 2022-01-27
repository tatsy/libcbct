#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_RAW_VOLUME_EXPORTER_H
#define LIBCBCT_RAW_VOLUME_EXPORTER_H

#include "BaseExporter.h"

class LIBCBCT_API RawVolumeExporter : public BaseExporter {
public:
    RawVolumeExporter() = default;
    ~RawVolumeExporter() = default;

    void write(const std::string &filename, FloatVolume &tomogram) const override;
};

#endif  // LIBCBCT_RAW_VOLUME_EXPORTER_H
