#include "libcbct.h"
#include "Reconstruction/FilterUtils.h"

int main() {
    const libcbct::Feldkamp fdk(libcbct::Backend::Cpu);
    libcbct::CpuExecutionSession session;
    const auto buffer = session.allocate<float>(std::array<std::size_t, 2>{ 2, 2 });
    const libcbct::VolumeF32 volume(2, 2, 2);
    const auto description = libcbct::STR_FMT("voxels={}, fft={}", volume.numel(),
                                             libcbct::paddedFFTLength(volume.size<0>()));
    return description == "voxels=8, fft=4" ? 0 : 1;
}
