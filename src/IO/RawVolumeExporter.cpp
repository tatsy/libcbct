#define LIBCBCT_API_EXPORT
#include "RawVolumeExporter.h"

#include <iostream>
#include <fstream>

void RawVolumeExporter::write(const std::string &filename, FloatVolume &tomogram) const {
    std::ofstream writer(filename.c_str(), std::ios::out | std::ios::binary);
    if (writer.fail()) {
        LIBCBCT_ERROR("Failed to open file: %s", filename.c_str());
    }

    const int sizeX = tomogram.size<0>();
    const int sizeY = tomogram.size<1>();
    const int sizeZ = tomogram.size<2>();

    float *buffer = new float[sizeX];
    for (int z = 0; z < sizeZ; z++) {
        for (int y = 0; y < sizeY; y++) {
            for (int x = 0; x < sizeX; x++) {
                buffer[x] = tomogram(x, y, z);
            }
            writer.write((char *)buffer, sizeof(float) * sizeX);
        }
    }
    delete[] buffer;
    writer.close();
}
