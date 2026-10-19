# CPU operator migration comparison

Measured on 2026-10-19 against the preserved `FeldkampCpu` implementation at
`71fa9d6`. The new path is `Feldkamp(Backend::Cpu, filter)`. The legacy class is
still independent and has not been converted to a wrapper.

## Reproduction

Configure a Release CPU build with tests and OpenMP enabled, then run:

```sh
cmake --build build/cpu --config Release --target cbct_fdk_cpu_benchmark --parallel
OMP_NUM_THREADS=4 build/cpu/bin/cbct_fdk_cpu_benchmark > progress.log 2> results.csv
```

The benchmark performs one warm-up per implementation, then seven alternating
runs and reports median wall time. Timing includes session/plan/coefficients and
buffer preparation, reconstruction, and progress reporting. Input generation is
excluded. Do not run other tests/builds concurrently with the measurements.
Numerical comparison checks every output for finiteness and requires maximum
absolute error below `1e-6`; timings have no automatic pass/fail threshold.

Environment: Linux x86-64 KVM guest, AMD EPYC 9V74, five visible virtual CPUs,
GCC 14.2.0, CMake Release, OpenMP enabled, `OMP_NUM_THREADS=4`. CUDA and Vulkan
were disabled. The compiler's default SSE2 pocketfft batching was retained.

## Results

| Fixture | Filter | Legacy median (ms) | Operators median (ms) | New / legacy | Max absolute error |
| --- | --- | ---: | ---: | ---: | ---: |
| Two spheres, 128x128x180 input, 64 cubed output | Ram-Lak | 414.129 | 354.200 | 0.855 | 0 |
| Two spheres, 128x128x180 input, 64 cubed output | Shepp-Logan | 428.109 | 396.164 | 0.925 | 0 |
| Signed rectangular data, 17x19x7 input, 17x19x5 output | Ram-Lak | 0.203 | 0.151 | 0.743 | 0 |
| Signed rectangular data, 17x19x7 input, 17x19x5 output | Shepp-Logan | 0.171 | 0.119 | 0.700 | 0 |

These are shared-host measurements, not a general speedup guarantee. In
particular, the sub-millisecond rectangular cases are sensitive to scheduling
and timer noise. No timing regression was observed in this run.

## Allocation and transfer accounting

The standalone benchmark replaces ordinary C++ `new`/`new[]` in the executable.
On this Linux shared-library build, those replacements also observe the library's
ordinary C++ requests. Counts below are from the last measured reconstruction;
both filters gave the same counts for a given fixture.

| Fixture | Legacy C++ allocation calls | Operators C++ allocation calls | Legacy requested bytes | Operators requested bytes |
| --- | ---: | ---: | ---: | ---: |
| Two spheres | 8,276 | 544 | 1,711,496 | 1,536,780 |
| Rectangular | 223 | 56 | 26,798 | 23,455 |

Requested bytes are cumulative, not peak live memory. These counters do **not**
measure every process allocation: pocketfft's aligned `malloc` storage, runtime
allocations, and over-aligned C++ requests are excluded. Cross-DLL interception
is platform-dependent, so these allocation counts are specifically Linux results.

The new FFT adapter retains its plan and scalar/SIMD packing workspaces instead
of preparing a transform plan for every forward/inverse call. Native pocketfft
scratch allocations still occur during transform execution. Tests additionally
verify that the session's explicit owned-buffer count and byte count remain
unchanged across repeated weight/filter applications. Those session statistics
exclude FFT-native storage and borrowed input/output volumes.

Both CPU implementations use host memory throughout: host/device transfer volume
is zero. The new path borrows the sinogram and output volume and retains padded,
spectrum, filtered, coefficient, and FFT workspace buffers for the entire loop.

## Validation scope

The Release/OpenMP suite passes all 176 CTest entries. It includes the original
sphere attenuation checks, comparison with the legacy CPU output and an
independent direct-convolution oracle, and direct operator tests for FFT
normalization, partial SIMD batches, detector boundaries, overwrite/accumulation,
shape/context/alias rejection, and view ownership. The GPU classes and kernel
sources are unchanged; no GPU performance or platform validation is claimed by
this CPU migration.

A separate Debug build with OpenMP disabled and
`-fsanitize=address,undefined -fno-omit-frame-pointer` passed all 46 selected
operator, FDK-contract, input-validation, and public-header tests. It used
`UBSAN_OPTIONS=halt_on_error=1` and `ASAN_OPTIONS=detect_leaks=0`: LeakSanitizer
could not run under the execution environment's ptrace restrictions, so leak
detection is not included in this result.
