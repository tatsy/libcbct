# LibCBCT

[![Build](https://github.com/tatsy/libcbct/actions/workflows/build.yaml/badge.svg)](https://github.com/tatsy/libcbct/actions/workflows/build.yaml)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

## Get started

### Build

```shell
git clone https://github.com/tatsy/libcbct.git --depth 1 --submodules
cd libcbct
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### Slang compiler

Enable Slang with `LIBCBCT_WITH_SLANG` (disabled by default):

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLIBCBCT_WITH_SLANG=ON
cmake --build build --config Release --parallel
```

CMake fetches [Slang v2025.24.3](https://github.com/shader-slang/slang/tree/v2025.24.3)
and its required submodules, then builds the compiler API (`slang::slang`) and
command-line compiler (`slangc`). The `slang-glslang` support library and its
glslang/SPIRV-Tools dependencies are included for SPIR-V optimization and
validation. Git and network access are required on the first
configure. Slang's tests, examples, graphics libraries, and LLVM-based CPU targets
are disabled. Slang can be built without a CUDA toolkit; compiling kernels to PTX
later requires CUDA's NVRTC library at runtime.

This option prepares the compiler dependency for kernel migration. Reconstruction
continues to use the existing CPU or CUDA implementation selected by
`LIBCBCT_WITH_CUDA`.

### Slang FDK kernels

`src/Reconstruction/Slang/FeldkampKernels.slang` provides three compute entry points:

| Entry point | Operation | Dispatch groups (block size: 16 × 16 × 1) |
| --- | --- | --- |
| `weightAndPadKernel` | Cosine weighting and zero padding | `ceil(nPad / 16) × ceil(detHeight / 16) × 1` |
| `applyFilterKernel` | Multiply both complex FFT components by the response | `ceil(nFreq / 16) × ceil(detHeight / 16) × 1` |
| `backProjectionKernel` | Bilinear sampling and accumulation into the volume | `ceil(volSize.x / 16) × ceil(volSize.y / 16) × volSize.z` |

With `LIBCBCT_WITH_SLANG=ON`, the default build generates CUDA source (`.cu`),
SPIR-V (`.spv`), and target-specific reflection (`.json`) for each entry point
under `build/kernels/<configuration>/`. To build these artifacts explicitly:

```shell
cmake --build build --config Release --target cbct_slang_kernels --parallel
```

On a host with NVRTC available, generate PTX explicitly:

```shell
cmake --build build --config Release --target cbct_slang_ptx --parallel
```

Additional `slangc` arguments can be supplied through the semicolon-separated
`LIBCBCT_SLANG_PTX_OPTIONS` cache variable. For example,
`-DLIBCBCT_SLANG_PTX_OPTIONS="-Xnvrtc;-arch=sm_75"` selects a CUDA architecture
through NVRTC.

Each CUDA entry point takes its corresponding Slang parameter structure by value.
Use its generated CUDA source and reflection to construct the host-side layout;
the arguments differ from the original CUDA kernel signature. Structured buffers
carry a device pointer and element count. `spectrum` uses `float2` elements with
the same real/imaginary storage as `cufftComplex`. Backprojection retains 64-bit
volume indexing; its SPIR-V requires the Vulkan `shaderInt64` feature.

The kernels retain voxel and detector pixel centers, half-pixel bilinear border
extrapolation, and the `(SOD / L)^2` distance weight. Detector dimensions must be
at least two for bilinear sampling. Keep the existing cuFFT pipeline and pass
the response divided by `nPad`, as in `FeldkampCUDA::reconstruct`. Initialize the
volume before reconstruction, use `scale = pi / nProj`, and dispatch projection
angles in order; volume accumulation is not atomic.

Reconstruction currently dispatches the original CUDA kernels. The Slang entry
points are available for correctness and performance comparison before switching
dispatch. With `LIBCBCT_BUILD_TESTS=ON`, `cbct_slang_kernel_tests` runs C++ code
generated from the Slang source and compares it against the existing CPU helpers,
including partial tiles, complex filtering, detector borders, and accumulation.

### Run

```shell
./cbct_exe -c /path/to/data/config.json
```

### File structure

```text
/path/to/data/ +- config.json         # Configuration file
               +- projections/        # Folder containing projection images
                 +- proj_0000.tif     # Projection image at angle 0
                 +- proj_0001.tif     # Projection image at angle 1
                 +- ...
               +- output/             # Folder to save reconstructed volume
```

### Example configuration file (JSON format)

```json
{
  "SOD": 99.19738,
  "SDD": 449.75,
  "PixelSizeX": 0.178,
  "PixelSizeY": 0.178,
  "DetWidth": 896,
  "DetHeight": 896,
  "NumberOfProj": 1000,
  "Clockwise": true
}
```

## License

[CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/), 2023-2025 (c) Tatsuya Yatagawa
