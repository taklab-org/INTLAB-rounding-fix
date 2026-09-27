# INTLAB-rounding-fix

[日本語](README.ja.md)

An experimental macOS arm64 workaround for directed-rounding failures in Apple Accelerate BLAS matrix multiplication, investigated with MATLAB R2026a and INTLAB.

**The patch is a C dynamic library for Apple Accelerate.** MATLAB and INTLAB are its motivating use case; the library itself does not depend on either. Bash scripts build the C sources, launch MATLAB and run tests. MATLAB functions provide the optional MATLAB/INTLAB checks. Python is not required.

The patch forwards the caller's rounding direction to Accelerate's internal BLAS callbacks and restores each worker's previous direction afterward. **It uses Accelerate's own parallel kernels.** On non-SME hardware, ABI 2 selects its CPU path instead of the accelerator path that failed the M3 Max rounding tests. It does not replace multiplication with a single-threaded implementation.

This is a community workaround, not an Apple, MathWorks or INTLAB release. It uses an exported, non-public dispatch entry and must be revalidated after OS or MATLAB changes. See [coverage and limitations](docs/validation.md).

## M3 Max repair (ABI 2)

The earlier ABI 1 library loads on the tested M3 Max but fails containment even without MATLAB. ABI 2 adds BLAS-scoped accelerator selection and rounding propagation through `dispatch_apply`. Rebuild both the dylib and MEX, then start a **fresh** MATLAB with `scripts/matlab.sh`. An already running process cannot adopt the new kernel selection.

M3 Max / macOS 26.6.2 / R2026a Update 2 / INTLAB V13 passed the recorded real-double tests, including two process workers. This does not establish a general Update 2 defect or validate every INTLAB operation. CPU fallback has a performance cost. See [diagnosis, evidence and limits](docs/m3-max-validation.md).

## Automatic selection and updating

The launcher injects the patch before MATLAB starts. The library then chooses the BLAS policy from CPU capabilities; no CPU-model option or manual `DYLD_INSERT_LIBRARIES` setting is needed:

| Detected capabilities | Automatic behavior |
|---|---|
| No SME/SME2, accelerator selector present (tested M3 Max) | Select Accelerate's CPU kernels and propagate rounding through their parallel callbacks |
| SME/SME2 present | Preserve Accelerate's native selection and propagate callback rounding |
| No accelerator selector | Leave the selection unchanged and propagate callback rounding |

This is capability-based selection, not certification of every CPU matching a row. ABI 2 numerical validation covers the recorded M3 Max environment; the SME branch has a policy regression test but has not been numerically rerun on M4.

For an existing checkout, run from its root:

```sh
git pull --ff-only
./scripts/build.sh
./scripts/test_native.sh
./scripts/matlab.sh --check
./scripts/matlab.sh
```

Rebuild both binaries after pulling: Git does not replace the locally generated dylib or MEX. The launcher rejects build metadata from ABI 1 with a rebuild instruction. `--check` prints `ABI=2 cpuFallback=1` on the tested M3 Max, with zero downward/upward violations. `cpuFallback=0` means the native selection was retained; judge acceptance by the numerical checks too. Start subsequent computations with this launcher in a fresh process.

## Requirements

- An Apple Silicon Mac running a compatible macOS. Recorded validation used macOS 26.6.2: ABI 1 on M4 Pro, ABI 2 on M3 Max. ABI 2 has not been rerun on M4 hardware.
- Bash (the macOS-provided version is sufficient) and Apple Command Line Tools or Xcode with a usable macOS SDK.
- An Apple Silicon MATLAB installation for MATLAB checks; R2026a Update 2 was tested on M3 Max; earlier ABI 1 tests used Update 5 on M4 Pro.
- A separately installed INTLAB only for the optional INTLAB tests.

No MATLAB, INTLAB or Apple framework binaries/headers are included. Build products stay in ignored `build/`. Native C tests do not require MATLAB or INTLAB.

## Quick start

Clone the repository, then build from its root:

```sh
git clone https://github.com/taklab-org/INTLAB-rounding-fix.git
cd INTLAB-rounding-fix
./scripts/build.sh
./scripts/test_native.sh
./scripts/matlab.sh --check
./scripts/matlab.sh
```

The last command starts a MATLAB desktop with the patch loaded. To run a batch job:

```sh
./scripts/matlab.sh -batch "your_function"
```

The launcher preserves the calling directory unless `-sd` is supplied. It adds the repository's MATLAB functions and build directory to `MATLABPATH`, but does not initialize INTLAB or replace your normal `startup.m`. `--check` deliberately uses an isolated test directory with a no-op startup.

MATLAB discovery prefers `/Applications/MATLAB_R2026a.app`; otherwise it accepts a single MATLAB app under `/Applications`. For a different location, use:

```sh
./scripts/build.sh --matlab "/path/to/MATLAB.app"
```

The location is saved only in `build/matlab-root.txt`. You may also set `MATLAB_ROOT`, or override the launcher with `--matlab`. Rebuild the MEX if switching MATLAB installations. Compiler/SDK discovery uses `xcrun`, with the standard Command Line Tools paths as a fallback; `build.sh --cc ... --sdk ...` supports explicit locations. The scripts do not download tools or accept licenses.

When updating from the earlier Python tools, rebuild with `./scripts/build.sh` to create the new plain-text local configuration.

## Verify the actual computation

In the patched MATLAB, at the BLAS thread setting intended for your work:

```matlab
report = rounding_patch_check(512);
report.patch
```

The check verifies the actual BLAS name, loaded patch ABI and an exact matrix witness whose entries are `1 + 2^-54`. The lower result must be at most `1`, and the upper result at least `1 + 2^-52`. The caller's rounding direction is restored afterward.

The BLAS name alone cannot distinguish patched and unpatched Accelerate. A successful INTLAB startup or scalar check is also insufficient. Repeat this test in **each process worker** and at representative workload sizes. The check does not force a particular thread count, and passing with a single-thread limit does not validate parallel rounding.

For optional diagnostics, start with:

```sh
ACCELERATE_ROUNDING_AUDIT=1 ./scripts/matlab.sh --check
```

The patch then reports intercepted BLAS calls by dispatch API, incoming rounding mismatches, callback thread IDs and overlapping callbacks. `report.patch.cpuFallbackSelected` records whether BLAS selected the CPU fallback; capability query counters are collected even with auditing disabled. Callback counters are disabled by default; zero callback counts with auditing disabled do not mean the library failed to load.

## Tests

Native-only build and tests:

```sh
./scripts/build.sh --native-only
./scripts/test_native.sh
```

The native runner compares separate unpatched and patched processes. A baseline rounding failure is recorded rather than treated as a runner failure. Patched tests must pass and must intercept callbacks on BLAS worker threads. A baseline pass on another OS is possible and is not evidence that the patch is needed there. Raw logs and machine-specific results stay in `build/`.

For a native executable that uses Accelerate, set the interposer immediately before executing it from Bash. For example, after the native-only build:

```sh
(export DYLD_INSERT_LIBRARIES="$PWD/build/libaccelerate_rounding.dylib"; exec ./build/witness)
```

The native test runner also verifies overlapping worker callbacks with audit counters enabled. Other applications must permit dynamic-library injection; protected launch intermediaries can remove `DYLD_*` variables. Validate each application's actual arithmetic and worker interception. The MATLAB launcher handles its release-specific launch shell separately.

Build with MATLAB support, then run its tests from the repository root:

```sh
./scripts/build.sh
./scripts/matlab.sh -sd "$PWD/tests" -batch run_matlab_tests
```

To include the original `testmm.m`, explicitly provide a private or otherwise exclusively owned INTLAB installation:

```sh
INTLAB_ROOT="/path/to/private/Intlab" \
  ./scripts/matlab.sh -sd "$PWD/tests" -batch run_matlab_tests
```

This optional test calls `startintlab`, which can write INTLAB's cache. Do not point it at a shared runtime being initialized or used by another job. It runs `testmm(288/512/540/1024)`, full-output signed point-interval checks and independent sampled nonpoint-interval checks at several thread limits. Without `INTLAB_ROOT`, INTLAB tests are skipped explicitly. Formal proof projects should retain their own bootstrap and cache ownership rather than use this test initializer.

## How it works

The patch interposes `dispatch_apply_with_attr` and `dispatch_apply` only when the immediate caller is `libBLAS.dylib`. It captures the caller's rounding direction per call, applies it around each original callback, and restores the worker's previous direction. Iterations, queues, attributes and indices are forwarded intact.

On non-SME hardware, it also filters the accelerator-selector field returned by `_get_cpu_capabilities` **only to libBLAS**. This selects Accelerate's own CPU kernels and their native parallel partitioning; it does not supply a replacement GEMM or force single threading. The selection is cached for the lifetime of the process and affects nearest-rounding performance too. SME/SME2 capabilities are preserved, so the M4 path is intended to remain unchanged, but ABI 2 has not been retested on M4. The selector is an undocumented implementation detail verified on the recorded OS, not a public Apple configuration API.

There is no global "current requested mode" shared between simultaneous products. Tests include separate callers requesting upward and downward rounding concurrently. The interposer source is in [src/rounding_interpose.c](src/rounding_interpose.c); provenance is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Scope and removal

The launcher selects `libmwAF_BLAS_ilp64.dylib` and this checkout's interposer before MATLAB starts, using a temporary launch rc. It does not edit your home startup files, launchd environment, application bundle, system libraries or signatures. It replaces the child MATLAB's `DYLD_INSERT_LIBRARIES` selection with this patch; composing other interposers is not tested. Existing MATLAB processes are unaffected.

To stop using this checkout's patch, close its MATLAB process and use your usual launcher. Any independently installed global configuration remains your own responsibility. Finder/Dock launches do not acquire this patch merely because the checkout exists. Do not set the interposer in the global launchd environment.

Limitations include the non-public dispatch ABI, other BLAS paths and data types, untested extreme values, and floating-point controls other than rounding direction. A numerical smoke test is not a certificate for a complete interval computation. Detailed measured results and version distinctions are in [docs/validation.md](docs/validation.md).

## Contributing and publication

Compatibility reports should include CPU, macOS build, MATLAB version/update, BLAS name, thread limit, tested sizes and the patch revision. An issue template is provided. Remove personal paths before posting logs.

The source now uses patch ABI 2. GitHub release tags, when present, identify a particular source revision; build and launch scripts never publish releases. See the [release checklist](docs/releasing.md).

Project code is provided under the [MIT License](LICENSE). External dependencies retain their own terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
