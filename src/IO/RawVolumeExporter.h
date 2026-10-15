#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_RAW_VOLUME_EXPORTER_H
#define LIBCBCT_RAW_VOLUME_EXPORTER_H

#include <iostream>
#include <fstream>

#include "BaseExporter.h"
#include "Utils/Normalization.h"

namespace libcbct {

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
        // Validate normalized output before opening/truncating the destination file.
        const auto [minVal, maxVal] = normalize ? detail::finiteMinMax(tomogram) : std::pair{ 0.0f, 0.0f };
        std::ofstream writer(filename.c_str(), std::ios::out | std::ios::binary);
        if (writer.fail()) {
            LIBCBCT_ERROR("Failed to open file: %s", filename.c_str());
        }

        const int sizeX = tomogram.size<0>();
        const int sizeY = tomogram.size<1>();
        const int sizeZ = tomogram.size<2>();

        auto buffer = std::make_unique<T[]>(sizeX * sizeY);
        for (int z = 0; z < sizeZ; z++) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    float value = tomogram(x, y, z);
                    if (normalize) {
                        if (maxVal == minVal) {
                            value = outMin;
                        } else {
                            const float range = maxVal - minVal;
                            // Preserve the existing FP32 formula unless its range overflows.
                            value = std::isfinite(range) ? (value - minVal) / range
                                : static_cast<float>((static_cast<double>(value) - minVal)
                                                     / (static_cast<double>(maxVal) - minVal));
                            value = outMin + value * (outMax - outMin);
                        }
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

}  // namespace libcbct

#endif  // LIBCBCT_RAW_VOLUME_EXPORTER_H
