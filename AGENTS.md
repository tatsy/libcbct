# AGENTS.md

## Overview

- This project is a C++ library for cone-beam CT reconstruction
- It will also provide a Python interface for the library

## Rule

- Without specific permission, do not edit the code in this repository

## Language

- Write code comments in English.
- Write commit comments also in English.

## C++ Namespaces

- Define library-owned C++ classes, functions, types, and constants in `libcbct`.
- Use `libcbct::detail` for implementation details needed in headers and anonymous namespaces for helpers local to a `.cpp` file.
- Keep namespace aliases and using directives out of public headers. Applications may use a local alias such as `namespace cbct = libcbct;`.
- Qualify library references inside macros so that they work from a consumer's namespace.
- Keep `main`, external ABI entry points, and third-party declarations in their required scopes. Shader entry-point names are independent of C++ namespaces.

## C++ Utilities

- Use the existing `kPi` family of APIs for mathematical constants; keep `<numbers>` usage inside `Common/Constants.h`.
- Logging and assertions are namespaced functions using compile-time checked format strings and `std::source_location` for caller diagnostics.
- Assertion arguments are evaluated even when `LIBCBCT_DISABLE_ASSERT` disables checking. Keep side effects out of assertion arguments.

## GPU Backend Development Roadmap

- Preserve CPU and CUDA backend support while migrating. CUDA executes Slang PTX and Vulkan executes Slang SPIR-V; keep GPU kernel sources in Slang rather than maintaining handwritten `.cu` implementations.
- **Phase 1 — Use Slang kernels through the CUDA backend.**
  - Keep the validated Slang FDK/backprojection kernels' reconstruction behavior and numerical conventions unchanged. The original handwritten CUDA kernels have been retired after numerical validation and a recorded performance comparison.
  - Compile Slang kernels to PTX and execute them through the existing CUDA backend. Reuse the current CUDA memory management and cuFFT integration where practical.
  - Verify future kernel changes against the CPU reference and established numerical fixtures, and benchmark execution time against the [recorded CUDA/PTX baseline](docs/benchmarks/cuda-slang-transition.md).
  - Keep the Slang code portable so that the same core compute kernels can later target SPIR-V.
- **Phase 2 — Add a Vulkan compute backend.**
  - Compile the validated Slang kernels to SPIR-V and implement Vulkan device, resource, pipeline, command submission, and synchronization management.
  - Integrate an appropriate Vulkan FFT implementation for FDK filtering, and avoid unnecessary CPU-GPU transfers.
  - Keep CUDA and CPU as supported backends; Vulkan is an additional backend, not an immediate replacement.
  - Detect optional GPU features at runtime, including FP32 atomic addition; provide a compatible fallback when an algorithm requires an unsupported feature.
  - Validate numerical results, performance, and platform compatibility (including macOS via MoltenVK) against the existing backends.
- Keep reconstruction algorithms separate from backend-specific execution and resource management, and share Slang kernels between backends where practical.
- Make GPU backends optional at build time so that CPU-only builds and tests remain available.

## Operator-Based Reconstruction Architecture

- Migrate toward reconstruction algorithms that compose reusable operators. A new reconstruction method should reuse existing operators without requiring a separate implementation of the entire method for every backend.
- Separate responsibilities into reconstruction recipes, operator implementations, and execution/resource management.
  - Reconstruction recipes define operation ordering and algorithm parameters; keep backend-specific branches, kernel launches, FFT library calls, memory allocation, transfers, and synchronization out of these recipes.
  - Give operators explicit, typed input/output contracts, including shapes, axis order, numerical types, geometry, normalization, and overwrite versus accumulation semantics.
  - Initially extract weighting with zero padding, projection filtering, and FDK backprojection. Keep FFT, spectral multiplication, and cropping as reusable components within projection filtering; preserve existing fused kernels where practical.
  - Preserve pixel-center conventions, interpolation and boundary behavior, FFT normalization, projection angles, and accumulation order across implementations. Backend-native FFT representations may differ internally while exposing the same mathematical results.
  - Name FDK-specific backprojection accordingly. Do not assume that its distance-weighted operation is a general adjoint projector; iterative methods may require a separately defined forward/adjoint pair.

### Backend Selection and Data Movement

- Select implementations per operator, with an overall backend setting supplying defaults and operator-specific settings providing overrides.
- Treat CPU, CUDA, and Vulkan as execution/resource backends. CUDA operators use Slang PTX with CUDA memory and cuFFT. The `slang-ptx` CLI backend name has been retired; use `cuda` and do not restore a compatibility alias for that CLI name.
- Use backend-neutral buffer/view interfaces carrying shape, type, layout, and device/context ownership. Keep raw CUDA and Vulkan handles inside backend adapters.
- Centralize allocation, transfers, synchronization, workspace reuse, FFT plans, and kernel/pipeline lifetimes in execution sessions and backend services.
  - Keep consecutive operations on the same device resident in device memory and reuse their stream or command batch. Avoid adding a transfer or host wait at every operator boundary.
  - Support mixed-backend pipelines by inserting required transfers and synchronization at backend boundaries. Initially use host staging for CUDA-Vulkan transfers; direct interop is a later optimization.
  - Keep the reconstruction volume on the backprojection backend throughout the FDK projection loop. Transfer filtered projections when necessary rather than moving the volume for every projection.
  - Reuse immutable weights and filter coefficients. Define ownership and update rules so that stale copies of mutable buffers cannot be reused.
  - Validate operator availability, context compatibility, device limits, and required features before execution. Report explicitly requested unsupported configurations clearly; fallback behavior must follow a defined selection policy.
- Keep Slang CUDA/PTX and SPIR-V parameter packing and reflection handling inside their respective adapters. Reconstruction recipes must not depend on shader ABI layouts.

### Incremental Migration and Validation

- First define operator contracts and minimal common buffer/execution interfaces, extract CPU operators, and implement one shared FDK recipe.
- Then adapt the existing CUDA/PTX and Vulkan implementations to the operator interfaces, followed by mixed-backend execution and per-operator command-line selection.
- Preserve existing reconstruction classes and their public APIs during migration. Convert them to thin configuration wrappers around the shared recipe after correctness and performance have been verified.
- Keep existing implementations as comparison references until their replacements are validated. Test individual operators, complete reconstruction, and mixed-backend combinations, including FFT normalization, interpolation boundaries, partial tiles, and accumulation.
- Preserve the existing FDK comparison target of maximum absolute error below `1e-6` on the established fixtures; choose additional operator tolerances according to their numerical behavior.
- Compare execution time, transfer volume, and allocation counts to detect regressions. Keep CPU-only builds and optional-backend configurations working throughout migration.
