#define LIBCBCT_API_EXPORT
#include "RawVolumeExporter.h"

#include <iostream>
#include <fstream>

void RawVolumeExporter::write(const std::string &filename, const VolumeF32 &tomogram, VolumeType type) const {
    switch (type) {
    case VolumeType::Uint8:
        writeAsType<uint8_t>(filename, tomogram, true, 0.0f, 255.0f);
        break;
    case VolumeType::Uint16:
        writeAsType<uint16_t>(filename, tomogram, true, 0.0f, 50000.0f);
        break;
    case VolumeType::Uint32:
        writeAsType<uint32_t>(filename, tomogram, true, 0.0f, 50000.0f);
        break;
    case VolumeType::Float32:
        writeAsType<float>(filename, tomogram, false);
        break;
    case VolumeType::Float64:
        writeAsType<double>(filename, tomogram, false);
        break;
    default:
        LIBCBCT_ERROR("Unsupported volume type for RAW export!");
        break;
    }
}