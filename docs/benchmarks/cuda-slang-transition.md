# CUDA to Slang PTX transition baseline

Measured before retiring the handwritten CUDA kernels at revision
`90817b850513d9479dbd15481d4d4f925fc394ea`.

- Platform: Windows, NVIDIA GeForce RTX 3080 Ti, CUDA Toolkit 12.4.
- Build: MSVC Release, locally installed Slang.
- Input: the existing two-sphere phantom; 128 x 128 detector, 180 projections,
  64 x 64 x 64 reconstruction, Shepp-Logan filter.
- One warm-up reconstruction per implementation, followed by three alternating
  Native CUDA / Slang PTX measurements in one process.
- Wall-clock times include allocations, module/FFT setup, transfers, execution,
  and synchronization. Sinogram generation is excluded.

| Implementation | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Median (ms) |
| --- | ---: | ---: | ---: | ---: |
| Native CUDA | 83.8809 | 85.5888 | 88.7649 | 85.5888 |
| Slang PTX | 160.115 | 149.511 | 145.638 | 149.511 |

The maximum absolute difference between reconstructed volumes was zero on this
fixture. Slang PTX took approximately 1.75 times the native implementation's
median time in this measurement. The source consolidation retains that measured
performance tradeoff; it does not establish performance parity. These results
are specific to this device, fixture, and setup, and do not isolate kernel-only
execution time.

The CUDA backend now executes the validated Slang PTX implementation. CPU and
Vulkan comparisons remain the numerical regression references. Future operator
refactoring should compare timings and transfer/allocation overhead against this
baseline as well as checking numerical results.
