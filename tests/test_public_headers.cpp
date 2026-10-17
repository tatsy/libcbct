#include "libcbct.h"
#include "Reconstruction/FilterUtils.h"

int main() {
    const libcbct::VolumeF32 volume(2, 2, 2);
    const auto description = libcbct::STR_FMT("voxels={}, fft={}", volume.numel(),
                                             libcbct::paddedFFTLength(volume.size<0>()));
    return description == "voxels=8, fft=4" ? 0 : 1;
}
