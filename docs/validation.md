# Validation and limits

## Current revision

ABI 2 adds the M3 Max repair and expands regression coverage. Its separate [M3 Max validation](m3-max-validation.md) records the new implementation, tests and performance limits. The M4 measurements below describe **ABI 1**, not a rerun of ABI 2. In particular, the original near-zero performance overhead does not apply to M3 CPU fallback.

## Original recorded environment

| Item | Recorded value |
|---|---|
| CPU | Apple M4 Pro, 12 cores |
| OS | macOS 26.6.2 (25G83) |
| MATLAB | R2026a Update 5, 26.1.0.3346908, MACA64 |
| BLAS | Apple Accelerate BLAS (ILP64) |
| INTLAB | V13, private prepared runtime |
| Date | 2026-09-13 |

The project author also reported a successful check on an Apple M4 MacBook Air. Its macOS/MATLAB versions and detailed results have not been supplied; it is a user report, not a second fully recorded test matrix. Names such as `2026a` identify the investigated MATLAB release, not a guarantee covering all its supported operating systems.

## Original investigation

- Unchanged `testmm(n)` passed for every n from 2 through 287 and first failed at 288 on the M4 Pro. The Air's originally reported failure threshold was 540.
- Unpatched Accelerate had interval-enclosure failures in 224 of 870 cases across thread limits 12, 1, 2, 4, 12. Patched Accelerate had zero raw downward, raw upward or interval-enclosure failures in that comparison.
- The patch passed original `testmm(288/512/540/1024)` with Accelerate's internal parallelism enabled. The original function checks nonzero interval radii; separate tests check actual interval endpoints.
- A native C reproduction, without MATLAB or INTLAB, reproduced the fault and passed with the patch.
- Callback-entry instrumentation directly observed incoming rounding modes differing from the calling thread's requested mode. The MATLAB audit recorded 10,344 intercepted calls, 25,230 callbacks, 14,871 incoming mismatches, four distinct callback thread IDs and a peak of three overlapping callbacks.
- Two MATLAB process workers passed loaded-patch and 512-size interval checks at BLAS thread limit 2. This proves those tested worker launch paths, not all MATLAB cluster launchers.

INTLAB V13's inspected multithreaded self-test used size 220, below the measured threshold. Its public rounding-check result also omitted a separate internal multithreading flag. Thus a passing INTLAB startup check did not contradict the failing matrix tests. Other INTLAB versions should be inspected and tested independently.

## Tests included here

The public source layout was rebuilt and tested independently, including a clean copy under a directory containing spaces. Native tests, MATLAB checks at five sizes, and the optional INTLAB testmm/containment tests passed. A sanitized, machine-readable summary is in [public-layout-validation.json](public-layout-validation.json). At that validation date, the interposer numerical source was unchanged from the original patch; ABI 2 subsequently changed kernel selection and added an ordinary apply wrapper.

After replacing the Python utilities with macOS system Bash 3.2.57, the native and MATLAB/INTLAB tests above were rerun successfully. A clean copy under a path containing spaces passed native-only and MATLAB builds using the system `PATH`, native tests, and the MATLAB 512-size check. Native audit peaks were three overlapping callbacks for the witness and four for the stress test; the clean-copy MATLAB check observed two. Separate launcher interface checks covered quoted paths, argument forwarding, exit status and temporary-rc cleanup. The C interposer was not changed by this migration.

`witness.c` checks all output entries for exact products with values ±(1+2^-54), at 18 sizes from 2 through 2048, in both directed modes, and checks caller rounding restoration. It requests Accelerate-managed multithreading and never substitutes a manually partitioned or single-threaded product.

`stress.c` uses signed 53-bit integer numerators divided by 2^52. For 64 sampled outputs per product, it accumulates exact products independently in 128-bit integers and checks the directed BLAS result. It covers four shapes, all four transpose combinations, two rounding directions, and 40 calls from concurrent upward/downward callers. This samples 4,608 outputs, not every entry of every matrix.

`policy.c` covers 128 synthetic capability combinations, including preservation of SME/SME2 and unrelated bits. This verifies automatic selection logic, not numerical behavior on untested hardware. `scope.c` verifies that non-BLAS capability and dispatch callers remain untouched.

`run_matlab_tests.m` checks five sizes without INTLAB. If `INTLAB_ROOT` is supplied, it initializes that runtime, runs the unchanged `testmm.m` at four sizes, and checks a nonrepresentable exact interval product directly. No proprietary INTLAB code or runtime cache is included.

Passing the included tests is evidence for these cases. Tests for complex numbers, single precision, every BLAS routine/path, subnormals, overflow and arbitrary formal proof workloads have not been completed. Only rounding direction is propagated; exception flags are not aggregated across workers and other floating-point controls are not synchronized.

## Original ABI 1 performance

The original benchmark used dense double DGEMM with exactly representable test sums, separate from the rounding-failure inputs. Each condition used three warmups, then seven groups of repeated products. Median times from two baseline and two patched runs were averaged; the order was baseline, patched, single-threaded, patched, baseline. Diagnostics were disabled. Other jobs on the machine were not stopped.

| n | Baseline parallel, upward | Patched parallel, upward | Single thread, upward |
|---|---:|---:|---:|
| 512 | 0.327 ms | 0.328 ms | 0.614 ms |
| 1024 | 2.640 ms | 2.620 ms | 7.088 ms |
| 2048 | 22.005 ms | 21.674 ms | 45.127 ms |
| 4096 | 170.876 ms | 169.594 ms | 362.793 ms |

Across nearest/upward conditions, measured time differences ranged approximately from -1.5% to +1.1%. Small negative differences should not be interpreted as a patch speedup. The patched parallel runs retained approximately 1.9–2.8 times the single-threaded throughput in this comparison. [Recorded CSV](benchmark-m4-pro.csv)

`tests/benchmark.c` is included for repetition. After building, run `build/benchmark 0` for normal parallel execution, `build/benchmark 1` for single-thread execution, and run the parallel program with `DYLD_INSERT_LIBRARIES` set to the absolute path of this checkout's built dylib for patched execution. Do not enable audit counters for timing comparisons.

## Revalidation

The exported SPI `dispatch_apply_with_attr` and the immediate `libBLAS.dylib` caller are implementation details. A future OS may change the ABI or select another path. A loaded library or unchanged BLAS name alone is not proof that calls were intercepted or arithmetic is correct. Run numerical tests after changes and use audit mode to establish worker interception. Keep process-worker initialization/cache ownership with the consuming project.
