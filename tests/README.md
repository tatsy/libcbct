# Utility regression tests

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
