# Utility regression tests

## FDK operator migration baseline

`test_fdk_contracts.cpp` runs the numerical contracts on both the preserved
`FeldkampCpu` and the new `Feldkamp(Backend::Cpu)` implementation. Run it with:

```sh
cmake --build build/cpu --config Release --parallel
ctest --test-dir build/cpu -C Release -R 'FdkContracts|FdkSamplingContracts' --output-on-failure
```

The tests call the production APIs (`FeldkampCpu`, `Feldkamp`, `makeRampResponse`,
`makeCosineWeights`, and `bilerp`). `FdkReference.h` supplies a small double-precision
oracle based on direct spatial convolution and explicit geometric formulas. It
does not call the production FFT, filter, projection, or interpolation helpers.
Keep this oracle independent when the production implementation changes. Its
per-projection filtering and additive backprojection functions can also supply
expected values for future operator tests. It is intended for small, valid
fixtures, not arbitrary user inputs or reconstruction performance measurements.

Both ramp filters are covered. The tests check:

- Every frequency bin, including DC and Nyquist, against a direct DFT, with no
  inverse-FFT normalization included in the public ramp response.
- Pixel-center cosine weights with unequal detector pitches, and bilinear
  interpolation/extrapolation at all four half-pixel borders.
- A rectangular 17x19 detector, seven ordered views, asymmetric signed data,
  output shape, input preservation, and finite values. The maximum absolute
  difference from the independent FDK oracle must be below `1e-6`.
- Edge impulses and distinct rows, exercising zero extension, filtering, and
  cropping through the complete reconstruction path.
- An analytic isocenter value and `pi/N` scaling for one, two, and seven views.
- Detector-domain lower-edge inclusion, upper-edge exclusion, and zero output
  outside the detector.
- Additive contributions with projection slots retained, including zero views
  after nonzero views, and repeat calls with changed dimensions and geometry.

`test_cpu_operators.cpp` directly exercises padded-buffer overwrite, explicit
backprojection into a nonzero destination, independent DFT and normalized FFT
round trips (including SIMD tails), spectral multiplication, cropping,
shape/alias/context validation before writes, owned-view lifetimes, borrowed input
updates, and reuse of session buffers. `test_fdk.cpp` compares both filters on the
established sphere phantom and checks physical attenuation as well as maximum
absolute error below `1e-6`. GPU/mixed-backend execution and forward/adjoint
inner-product tests remain separate stages of the migration.

```sh
ctest --test-dir build/cpu -C Release -R 'CpuExecution|CpuWeightAndPad|CpuRealFft|CpuSpectralMultiply|CpuCrop|CpuProjectionFilterTest|CpuFdkBackprojection|FeldkampInput|OperatorRecipe' --output-on-failure
```

The opt-in `cbct_fdk_cpu_benchmark` target compares complete reconstructions and
ordinary C++ allocation requests without adding timing thresholds to CTest.
See [measurement instructions and results](../docs/benchmarks/cpu-operator-transition.md).

## Utility test workflow

Build the normal test targets, then run CTest (replace `build/cpu` with your
configured build directory):

```sh
cmake --build build/cpu --config Release
ctest --test-dir build/cpu -C Release --output-on-failure
```

`Contracts.*` tests build their own executable on demand and run it if compilation
succeeds. They are excluded from the default build so that an ill-formed template
does not prevent the other tests from running. A compiler error is a test failure;
compilation failure is never treated as success. Contract builds share a CTest
resource lock, allowing `ctest -j` without concurrent builds in the same tree.
For Windows, use the project's test preset to inherit its DLL search path.

Run an individual group while implementing a fix:

```sh
ctest --test-dir build/cpu -C Release -R '^Contracts.vec_geometry$' --output-on-failure
ctest --test-dir build/cpu -C Release -R 'CheckedSize|ParseNumber|SettingsNumbers' --output-on-failure
```

## Contracts to implement

The implementation is intentionally unchanged. Against `d310dd5`, the CPU Release
configuration with GCC 14.2 passes 114 of 127 CTest entries. The 13 failing entries
identify these pending changes:

| Test group | Required behavior |
| --- | --- |
| `Contracts.vec_constexpr_constructors` | Default, broadcast, converting, and component constructors, copy assignment, and both indexing overloads work in constant evaluation for dimensions 2, 3, and 4. |
| `Contracts.vec_constexpr_arithmetic` | Component-wise vector arithmetic, unary minus, equality, and inequality work in constant evaluation. |
| `Contracts.vec_scalar_left` | All four scalar-left operations support dimensions 2, 3, and 4, preserve operand order, and promote the result type. |
| `Contracts.vec_mixed_compound` | Compound assignment accepts a scalar of a different arithmetic type, retains the left vector's element type, and returns a reference to that vector. Integer results follow built-in compound assignment conversion rules. |
| `Contracts.vec_geometry` | Ordinary calls deduce the dimension of `dot`; `length` and nonzero `normalize` work for float and double vectors in all three dimensions. Double precision is retained. |
| `Contracts.volume_foreach` | The implementation accepts the value callbacks promised by its constraint, including a callback taking `float&&`. Empty volumes invoke no callbacks. |
| `DivRoundUp.*` (four integer types) | Nonnegative dividends and positive divisors work up to the maximum representable value without intermediate overflow. |
| `Volume.RejectsNegativeDimensionsEvenWithZeroExtent` | Construction and resize reject negative dimensions before conversion to `size_t`, including when another extent is zero. Rejected resize preserves the original data. |
| `SettingsNumbers.RejectsMalformed*` (two tests) | Existing numeric settings consume the entire string and reject empty, malformed, or out-of-range input. Defaults apply only to missing keys. |

The other tests cover each vector component, scalar promotion, assignment return
values, variadic size checks, numeric parsing, spans, zero extents, deep copying,
and resize exception safety. The original 55 CTest entries continue to pass.

`checkedProduct` validates every argument even if another is zero and checks each
intermediate product from left to right: `(max, 2, 0)` throws, while
`(0, max, 2)` succeeds. These tests preserve that existing contract.

Zero-vector normalization, invalid `divRoundUp` divisors, negative dividends,
and whitespace/NaN/infinity policy for numeric settings are deliberately left
unspecified by these tests. GPU execution is outside this test change.
