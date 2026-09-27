# M3 Max repair — ABI 2

## Conclusion and environment

On 2026-09-28, the ABI 2 patch passed the recorded real-double matrix containment checks on Apple M3 Max (14 cores), macOS 26.6.2 (25G83), MATLAB 26.1.0.3251617 (R2026a Update 2), Apple Accelerate BLAS (ILP64), and a private INTLAB V13 runtime. This is evidence for the tested operations and launch paths, not certification of all INTLAB computations.

The installed libBLAS UUID was `23402175-D2CF-3B08-88D0-AFBBCF775FEF`. Machine-specific paths, raw logs and private INTLAB files remain in ignored `build/`. No system library, application signature or global startup configuration was changed.

## Diagnosis

The ABI 1 library really was loaded. Its callbacks ran on multiple threads and recorded incoming rounding mismatches. Nevertheless, the 512-by-512 MATLAB upward witness failed at every one of its 262,144 entries. Its exact value is `1 + 2^-54`, so a binary64 upper bound must be at least `1 + 2^-52`.

The same failure occurred in a native C process without MATLAB or INTLAB. Native `BLASSetThreading(BLAS_THREADING_SINGLE_THREADED)` also failed the witness. Consequently, neither a missing library nor missing worker-rounding propagation alone explains this configuration, and MATLAB Update 2 is not required to reproduce the failure. These observations do not establish that all M3 systems or all Update 2 installations fail.

Inspection of the installed libBLAS initialization showed a four-bit accelerator selector extracted from `_get_cpu_capabilities()` at bits 27–30. On this non-SME host, masking that selector for **libBLAS callers only** changes the selected numerical path. This is consistent with avoiding the AMX path in favor of CPU kernels; it does not establish a general AMX hardware defect or a public configuration contract.

Two changes were necessary:

| Variant | Result on this host |
|---|---|
| Original ABI 1 (`dispatch_apply_with_attr` only) | Exact witness fails despite intercepted callbacks |
| Add ordinary apply/group wrappers, retain accelerator selection | Witness still fails |
| Mask accelerator selector, retain only the old attribute wrapper | Stress test has 2,065 violations among 4,608 sampled outputs; no intercepted apply callbacks |
| Mask selector and propagate rounding through ordinary `dispatch_apply` | Witness and stress tests pass |

`dispatch_group_async` was not needed by the tested route and is not interposed in the final patch. The fix retains the existing attribute wrapper and adds an ordinary apply wrapper. Both preserve the framework's work scheduling and restore the previous rounding direction after each callback. No replacement GEMM or single-thread workaround is supplied.

The selector is cached at BLAS initialization, so it is intentionally fixed for the process, including nearest-rounding products. Changing it per product would race with simultaneous callers. The filter leaves SME/SME2 machines unchanged by design; **ABI 2 has not been rerun on M4**.

Apple publishes the [capability function declaration and SME feature bits](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/arm/cpu_capabilities.h), but not the selector used here. The [dispatch implementation](https://github.com/apple-oss-distributions/libdispatch/blob/main/src/apply.c) is also relevant. These are implementation details requiring revalidation after OS changes.

## Verification

- Native exact positive/negative witness: all entries at 18 sizes (`2,7,8,16,17,31,64,127,128,255,256,287,288,512,513,540,1024,2048`), both directed modes, zero violations. Caller rounding restoration is checked.
- Native integer-oracle stress: 32 serial and 40 concurrent products, four shapes and all four transpose combinations, 4,608 sampled output checks, zero violations. Simultaneous callers request opposite rounding directions.
- Native scope regression: non-BLAS capability queries and both non-BLAS apply calls are left untouched. BLAS interception and parallel callback overlap are checked separately.
- Original unchanged INTLAB `testmm`: sizes 288, 512, 540 and 1024 passed. These radius checks are supplemented by actual endpoint containment below.
- Extended INTLAB: 224 products passed. Signed point-interval checks cover **42,817,320 output entries**; nonpoint signed rectangular interval products add 2,048 independent sampled endpoint checks. The latter reference uses small integer endpoint products/sums exactly representable in binary64, without a reference BLAS matrix multiplication.
- MATLAB requested thread limits: 1, 2, 4 and 14. Sizes: 2, 17, 127, 287, 288, 512, 540 and 1024. These values are MATLAB settings, not proof that Accelerate limits its worker count accordingly; audit output independently establishes parallel execution.
- Two fresh process workers: each loaded ABI 2, passed the INTLAB rounding self-test, raw directed products and actual interval containment at 512/requested limit 2 and 1024/requested limit 4. Each minimum initialization left the prepared private cache unchanged.

The final extended client audit recorded 442 ordinary apply calls, 3,838 callbacks, 3,321 incoming rounding mismatches, and 12 overlapping callbacks. Numerical results, rather than interception counts alone, are the acceptance evidence. Scalar/INTLAB public self-tests alone would have missed the original failure.

The complete repository MATLAB suite can be repeated with an exclusively owned runtime:

```sh
./scripts/build.sh
./scripts/test_native.sh
INTLAB_ROOT="/path/to/private/Intlab" \
  ./scripts/matlab.sh -sd "$PWD/tests" -batch run_matlab_tests
```

For process-worker repetition, retain the consuming project's initialization owner, suppress full automatic worker startup, prepare the cache serially, and call the reviewed minimum initializer on every fresh process. The local run used the workspace `intlab-startup` helpers; the machine-specific driver is retained in `build/run_m3_extended.m`.

## Performance and limits

CPU fallback retains native parallel execution, but loses the throughput of the original accelerator route. The original M4 ABI 1 claim of approximately unchanged performance does **not** apply here. A final baseline run followed by a patched run used three warmups and the median of seven repeated-product groups, with audit disabled and no overlapping task-owned computations. Other host jobs were not stopped. Upward-mode timings were:

| n | Unpatched parallel (ms) | ABI 2 parallel (ms) | Time ratio |
|---:|---:|---:|---:|
| 512 | 0.377 | 1.086 | 2.88× |
| 1024 | 3.012 | 6.878 | 2.28× |
| 2048 | 23.173 | 38.498 | 1.66× |
| 4096 | 188.299 | 277.917 | 1.48× |

The benchmark uses exactly representable sums; these baseline timings do not validate unpatched rounding. This is one paired measurement, not a performance guarantee. Full nearest/upward timings, source/build hashes and test summaries are in [the machine-readable result](m3-max-validation.json). Earlier exploratory runs overlapped validation and are excluded from this table.

Full BLAS/type coverage, complex arithmetic, single precision, subnormal/overflow behavior, every INTLAB function, thread-pool safety, other macOS versions and ABI 2 on M4 remain unverified. Formal proof workloads must still pass their project-owned numerical and provenance gates. This patch propagates rounding direction; it does not synchronize other floating-point controls or aggregate worker exception flags.

Use a fresh MATLAB launched through this checkout's `scripts/matlab.sh`. Rebuild the dylib and status MEX together. Other separately installed ABI 1 copies do not acquire this repair automatically. The source distribution adds 128 synthetic capability-policy regression cases and rejects stale ABI 1 launcher build metadata. See [publication validation](publication-validation.json) for the final packaged source checks; the earlier hashes in the investigation result identify the pre-packaging build.
