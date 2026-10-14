# AGENTS.md

## Overview

- This project is a C++ library for cone-beam CT reconstruction
- It will also provide a Python interface for the library

## Rule

- Without specific permission, do not edit the code in this repository

## Language

- Write code comments in English.
- Write commit comments also in English.

## GPU Backend Development Roadmap

- Preserve the working CPU and CUDA implementations while migrating. Do not replace the entire CUDA backend with Vulkan in a single step.
- **Phase 1 — Migrate existing CUDA compute kernels to Slang.**
  - Start with the existing FDK/backprojection kernels, keeping reconstruction behavior and numerical conventions unchanged.
  - Compile Slang kernels to PTX and execute them through the existing CUDA backend. Reuse the current CUDA memory management and cuFFT integration where practical.
  - Verify correctness against the existing CUDA and CPU implementations, and benchmark execution time before replacing or retiring the original kernels.
  - Keep the Slang code portable so that the same core compute kernels can later target SPIR-V.
- **Phase 2 — Add a Vulkan compute backend.**
  - Compile the validated Slang kernels to SPIR-V and implement Vulkan device, resource, pipeline, command submission, and synchronization management.
  - Integrate an appropriate Vulkan FFT implementation for FDK filtering, and avoid unnecessary CPU-GPU transfers.
  - Keep CUDA and CPU as supported backends; Vulkan is an additional backend, not an immediate replacement.
  - Detect optional GPU features at runtime, including FP32 atomic addition; provide a compatible fallback when an algorithm requires an unsupported feature.
  - Validate numerical results, performance, and platform compatibility (including macOS via MoltenVK) against the existing backends.
- Keep reconstruction algorithms separate from backend-specific execution and resource management, and share Slang kernels between backends where practical.
- Make GPU backends optional at build time so that CPU-only builds and tests remain available.
