#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_RAW_VOLUME_EXPORTER_H
#define LIBCBCT_RAW_VOLUME_EXPORTER_H

#include "BaseExporter.h"

class LIBCBCT_API RawVolumeExporter : public BaseExporter {
public:
    RawVolumeExporter() = default;
    virtual ~RawVolumeExporter() = default;

    void write(const std::string &filename, const VolumeF32 &tomogram,
               VolumeType type = VolumeType::Float32) const override;

private:
    template <typename T>
    void writeAsType(const std::string &filename, const VolumeF32 &tomogram, bool normalize = false,
                     float outMin = 0.0f, float outMax = 1.0f) const {
        std::ofstream writer(filename.c_str(), std::ios::out | std::ios::binary);
        if (writer.fail()) {
            LIBCBCT_ERROR("Failed to open file: %s", filename.c_str());
        }

        const int sizeX = tomogram.size<0>();
        const int sizeY = tomogram.size<1>();
        const int sizeZ = tomogram.size<2>();
        const auto [minVal, maxVal] = tomogram.getMinMax();

        auto buffer = std::make_unique<T[]>(sizeX * sizeY);
        for (int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    float value = tomogram(x, y, z);
                    if (normalize) {
                        value = (value - minVal) / (maxVal - minVal);
                        value = outMin + value * (outMax - outMin);
                    }
                    const T outValue = static_cast<T>(value);
                    buffer[y * sizeX + x] = outValue;
                }
            }
            writer.write(reinterpret_cast<char *>(buffer.get()), sizeof(T) * sizeX * sizeY);
        }
        writer.close();
    }
};

#endif  // LIBCBCT_RAW_VOLUME_EXPORTER_H
