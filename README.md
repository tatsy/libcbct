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

### VS Code and CMake presets

Use CMake 3.24 or newer with the CMake Tools and C/C++ extensions in VS Code.
Run `CMake: Select Configure Preset`, configure, and then select the matching
build and test presets. Presets for other operating systems are hidden.

| Configure preset | Backends | Build/test presets |
| --- | --- | --- |
| `msvc2022` | CPU | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `msvc2022-cuda` | CPU + CUDA | `msvc2022-cuda-debug`, `msvc2022-cuda-release` |
| `msvc2022-vulkan` | CPU + Vulkan | `msvc2022-vulkan-debug`, `msvc2022-vulkan-release` |
| `msvc2022-cuda-vulkan` | CPU + CUDA + Vulkan | `msvc2022-cuda-vulkan-debug`, `msvc2022-cuda-vulkan-release` |
| `macos-debug`, `macos-release` | CPU | Same as the configure preset |
| `macos-vulkan-debug`, `macos-vulkan-release` | CPU + Vulkan | Same as the configure preset |

Windows uses Visual Studio 2022 with x64. macOS uses Ninja and the system compiler;
its OpenMP option defaults to `OFF` and can be enabled locally when available.
All development presets enable tests. Slang is selected automatically for GPU
backends. Vulkan on macOS also requires a loader and MoltenVK installation.

Each configure preset uses `build/<preset-name>` for its cache and artifacts,
so backend and build-type selections do not reuse incompatible caches. The
old `build/CMakeCache.txt` is not reused by these presets.
Windows build types share their configure preset's directory because Visual
Studio supports multiple configurations in one build tree.

For example, build and test the Windows CUDA backend with:

```shell
cmake --preset msvc2022-cuda
cmake --build --preset msvc2022-cuda-release
ctest --preset msvc2022-cuda-release
```

Use `macos-vulkan-release` for all three commands on macOS with Vulkan.
Machine-specific dependency paths belong in the Git-ignored
`CMakeUserPresets.json`. For example, adapt these OpenCV paths for a local
Windows CPU build; `OpenCV_DIR` must contain `OpenCVConfig.cmake`:

```json
{
  "version": 5,
  "configurePresets": [
    {
      "name": "local-msvc2022",
      "inherits": "msvc2022",
      "cacheVariables": {
        "OpenCV_DIR": "C:/path/to/opencv/cmake"
      },
      "environment": {
        "PATH": "C:/path/to/opencv/bin;$penv{PATH}"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "local-release",
      "inherits": "Release",
      "configurePreset": "local-msvc2022"
    }
  ],
  "testPresets": [
    {
      "name": "local-release",
      "inherits": "Release",
      "configurePreset": "local-msvc2022"
    }
  ]
}
```

The Windows F5 launch configuration obtains the executable path from CMake Tools.
Select `cbct_exe` as the launch target and a matching build preset before launching;
F5 prompts for the input `config.json` path and runtime backend.
See the [CMake Tools preset guide](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md)
for the extension's configure, build, and test commands.

### C++ API

Library-owned C++ types, functions, and constants are defined in `libcbct`:

```cpp
#include <libcbct.h>

namespace cbct = libcbct;

cbct::VolumeF32 reconstruct(const cbct::VolumeF32 &sinogram, const cbct::Geometry &geometry) {
    return cbct::FeldkampCPU(cbct::RampFilter::SheppLogan).reconstruct(sinogram, geometry);
}
```

Namespace aliases are optional and belong in consumer code. Public headers do not
introduce global aliases for the former unqualified names. Update existing callers
to use qualified names (or local using declarations) and rebuild downstream C++
binaries, since moving public types and functions changes their ABI.
Macros retain their existing names; logging functions such as
`libcbct::LIBCBCT_INFO` are ordinary namespaced C++ functions.

### Vulkan backend

Enable Vulkan compute/reconstruction with `LIBCBCT_WITH_VULKAN` (disabled by
default). Slang is enabled automatically to build its SPIR-V kernels:

```shell
cmake -S . -B build/vulkan -DCMAKE_BUILD_TYPE=Release \
  -DLIBCBCT_WITH_VULKAN=ON \
  -DLIBCBCT_WITH_CUDA=OFF -DLIBCBCT_BUILD_TESTS=ON
cmake --build build/vulkan --config Release --parallel
ctest --test-dir build/vulkan -C Release --output-on-failure
./build/vulkan/bin/cbct_exe --backend vulkan -c /path/to/data/config.json \
  --size 512 --no-preview
```

Install Vulkan's loader and development headers separately, through the Vulkan SDK
or your platform's development package. CMake uses `find_package(Vulkan REQUIRED)`
and checks for `vulkan/vulkan.hpp` in those headers. A loader-only installation is
insufficient; Vulkan-Hpp must be included in the development environment. Vulkan-Hpp
is not fetched separately, keeping it aligned with the installed Vulkan headers.

CMake fetches [VMA v3.3.0](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/tree/v3.3.0)
and compiles its implementation in `cbct_vma`, an internal static library linked
by `cbct`. Vulkan-Hpp manages instances, devices, pipelines, descriptors, command
buffers, and fences. VMA supplies device and mapped staging allocations using the
context's loader proc-address functions. No volk dependency is needed.
To use an existing VMA checkout offline, set `FETCHCONTENT_SOURCE_DIR_VMA`.

`libcbct::FeldkampVulkan(kernelDirectory, filter, deviceIndex)` implements FDK using the
shared Slang weighting, frequency filtering, and backprojection kernels. Additional
Slang kernels perform a batched FP32 radix-2 FFT and crop the filtered projection.
The FFT handles the power-of-two row lengths produced by `paddedFFTLength`; its
full complex spectrum uses a mirrored ramp response divided by `nPad`, preserving
the existing inverse-transform normalization. Intermediate buffers and the volume
remain on GPU. Each projection is uploaded once and the final volume downloaded
once. Projections execute in order, with a fence wait before staging-buffer reuse.
This initial FFT favors a simple portable implementation over tuned FFT performance.

The runtime requires Vulkan 1.1 and support for 16 × 16 × 1 work groups. Generated
SPIR-V targets version 1.3 and uses checked 32-bit storage-buffer indexing, so FDK
does not require `shaderInt64` or FP32 atomic addition. One invocation owns each
voxel during a projection. Optional atomic support is reported by
`libcbct::VulkanComputeContext::deviceInfo()`/`availableDevices()`; atomic extensions are
not enabled by this adapter. Oversized buffers/dispatches are rejected according
to device limits, including `maxStorageBufferRange`. Large volumes require a
future tiled implementation or another backend.

`--vulkan-device` selects the physical device index; the default `-1` prefers a
discrete GPU. `--spirv-dir` overrides the kernel directory compiled into the
executable, for example after relocating it. Supply all seven `.spv` files and
their matching `.spirv.json` reflection files from `build/kernels/<configuration>`.
The adapter reads storage-buffer bindings and uniform offsets from reflection,
including nested geometry fields. CUDA and SPIR-V layouts are handled separately.

The host foundation is reusable independently of reconstruction:

- `Utils/VulkanCompute.h`: device selection, VMA buffers, SPIR-V pipelines,
  storage descriptor sets, push constants, transfers, barriers, and fence waits.
- `Utils/SlangSpirvKernel.h`: reflected set-0 structured-buffer bindings and named
  push-constant fields for compute kernels with `[numthreads(16, 16, 1)]`.
- `Utils/VulkanFft.h`: batched forward-real and inverse-complex GPU transforms.

Batches retain their buffers, kernels, and device until execution completes.
Submission is synchronous; do not change mapped input buffers while a batch is
in flight. Building the Vulkan backend requires Slang, but does not require CUDA.
At runtime, provide the generated SPIR-V/reflection files; Slang is not invoked.
With Vulkan disabled, no Vulkan SDK or VMA is required. The existing
CPU and CUDA backends remain available, and `auto` retains its existing
CUDA/CPU selection behavior.

Vulkan tests check staging transfers, resource lifetimes, FFT results against a
direct DFT, and both FDK filters against CPU/CUDA results, including rectangular
detectors and partial tiles. GPU tests skip when no compatible driver/device is
available. Portability enumeration and `VK_KHR_portability_subset` are enabled
when advertised for MoltenVK; the current implementation has been tested on
Windows/NVIDIA, with macOS/MoltenVK validation still pending.

### Slang compiler

Slang is enabled automatically when `LIBCBCT_WITH_CUDA` or
`LIBCBCT_WITH_VULKAN` is `ON`. `LIBCBCT_WITH_SLANG` is derived internally and is
no longer a user-configurable option. For example, to build the CUDA backend:

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLIBCBCT_WITH_CUDA=ON
cmake --build build --config Release --parallel
```

CMake first looks for a local Slang installation (including `slang_ROOT` or the
Vulkan SDK). Otherwise it fetches
[Slang v2025.24.3](https://github.com/shader-slang/slang/tree/v2025.24.3)
and its required submodules, then builds the compiler API (`slang::slang`) and
command-line compiler (`slangc`). The `slang-glslang` support library and its
glslang/SPIRV-Tools dependencies are included for SPIR-V optimization and
validation. Git and network access are required on the first
configure. Slang's tests, examples, graphics libraries, and LLVM-based CPU targets
are disabled. Vulkan-only builds do not need a CUDA toolkit; compiling kernels
to PTX requires CUDA's NVRTC library during the build.

`LIBCBCT_WITH_CUDA=ON` automatically enables `LIBCBCT_WITH_SLANG` and builds the
PTX artifacts needed by `libcbct::FeldkampCUDA`. CUDA kernels are maintained in Slang;
there are no handwritten `.cu` sources or CUDA-language compilation steps.
The CUDA Toolkit, including NVRTC, is required to build this backend. CUDA Runtime,
Driver API, and cuFFT remain its execution dependencies. CPU-only builds remain
available without Slang or the CUDA Toolkit.

### Slang FDK kernels

`src/Reconstruction/Slang/FeldkampKernels.slang` provides three compute entry points:

| Entry point | Operation | Dispatch groups (block size: 16 × 16 × 1) |
| --- | --- | --- |
| `weightAndPadKernel` | Cosine weighting and zero padding | `ceil(nPad / 16) × ceil(detHeight / 16) × 1` |
| `applyFilterKernel` | Multiply both complex FFT components by the response | `ceil(nFreq / 16) × ceil(detHeight / 16) × 1` |
| `backProjectionKernel` | Bilinear sampling and accumulation into the volume | `ceil(volSize.x / 16) × ceil(volSize.y / 16) × volSize.z` |

With either GPU backend enabled, the default build generates diagnostic CUDA source (`.cu`),
SPIR-V (`.spv`), and target-specific reflection (`.json`) for each entry point
under `build/kernels/<configuration>/`. These `.cu` files are generated outputs,
not maintained sources or inputs to an NVCC build. To build these artifacts explicitly:

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
volume indexing for CUDA/PTX. The generated SPIR-V uses 32-bit indexing with
host-side range checks and does not require Vulkan `shaderInt64`.

The kernels retain voxel and detector pixel centers, half-pixel bilinear border
extrapolation, and the `(SOD / L)^2` distance weight. Detector dimensions must be
at least two for bilinear sampling. Keep the existing cuFFT pipeline and pass
the response divided by `nPad`, as in `libcbct::FeldkampCUDA::reconstruct`. Initialize the
volume before reconstruction, use `scale = pi / nProj`, and dispatch projection
angles in order; volume accumulation is not atomic.

`libcbct::FeldkampCUDA(kernelDirectory, filter)` loads the three PTX files once per
reconstruction and executes them using CUDA's primary context. Device buffers,
cuFFT plans, copies, and kernel launches share one CUDA stream; each projection
completes before the next one. `libcbct::FeldkampCUDA(filter)` uses the kernel directory
configured for the library build. Resources are released after queued work completes, including on
exceptions.

`libcbct::FeldkampSlangCUDA` remains a source-compatible alias of `libcbct::FeldkampCUDA`; both names
execute the same implementation. Rebuild downstream C++ binaries against the
updated headers/library. The handwritten CUDA implementation has been retired;
its pre-retirement [numerical and performance comparison](docs/benchmarks/cuda-slang-transition.md)
records the measured tradeoff on a small phantom.

Supply each `<entry>.ptx` together with its matching `<entry>.ptx.json` reflection
file. The host validates parameter sizes, field offsets (including geometry
padding), entry points, and thread group sizes before loading each module. The
adapter expects the by-value structured-buffer ABI generated by Slang v2025.24.3;
an incompatible local compiler layout produces an error instead of launching
with incorrect arguments. These kernels do not require `SLANG_globalParams`.

With `LIBCBCT_BUILD_TESTS=ON`, `cbct_slang_kernel_tests` runs C++ code
generated from the Slang source and compares it against the existing CPU helpers,
including partial tiles, complex filtering, detector borders, and accumulation.
It also checks host layouts against generated CUDA reflection without a GPU.
CUDA builds register PTX module tests and compare
full reconstruction against CPU for both ramp filters,
including a rectangular detector and partial tiles. GPU tests skip when no CUDA
device is available.

### Run

```shell
./cbct_exe -c /path/to/data/config.json
```

Select a reconstruction backend with `--backend` (or `-b`):

| Backend | Behavior |
| --- | --- |
| `auto` (default) | CUDA/PTX when built and a device is available; otherwise CPU |
| `cpu` | CPU reconstruction |
| `cuda` | Slang PTX kernels and cuFFT; requires a CUDA build and device |
| `slang-ptx` | Compatibility alias for `cuda`, with identical PTX-directory handling |
| `vulkan` | Slang SPIR-V kernels and GPU FFT; requires a Vulkan build and device |

Explicitly requesting a backend that was not built produces an error. For example:

```shell
cmake -S . -B build/gpu -DCMAKE_BUILD_TYPE=Release \
  -DLIBCBCT_WITH_CUDA=ON -DLIBCBCT_BUILD_TESTS=ON
cmake --build build/gpu --config Release --parallel
./build/gpu/bin/cbct_exe -c /path/to/data/config.json --backend cuda \
  --ptx-dir ./build/gpu/kernels/Release
ctest --test-dir build/gpu -C Release --output-on-failure
```

`--ptx-dir` defaults to the configured build's kernel directory. When moving the
executable, copy the PTX and reflection files together and pass their new directory.
The library executes precompiled PTX and does not invoke Slang or NVRTC at runtime.

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
