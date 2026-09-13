# INTLAB-rounding-fix

[日本語](README.ja.md)

An experimental macOS arm64 workaround for directed-rounding failures in Apple Accelerate BLAS matrix multiplication, investigated with MATLAB R2026a and INTLAB.

The patch forwards the caller's rounding direction to Accelerate's internal BLAS callbacks and restores each worker's previous direction afterward. **It preserves Accelerate's own matrix partitioning, kernels and parallel execution.** It does not replace multiplication with a single-threaded implementation.

This is a community workaround, not an Apple, MathWorks or INTLAB release. It uses an exported, non-public dispatch entry and must be revalidated after OS or MATLAB changes. See [coverage and limitations](docs/validation.md).

## Requirements

- An Apple Silicon Mac running a compatible macOS. The complete recorded validation used macOS 26.6.2 and an M4 Pro.
- Python 3.9 or later and Apple Command Line Tools or Xcode with a usable macOS SDK.
- An Apple Silicon MATLAB installation for MATLAB checks; R2026a Update 5 was tested.
- A separately installed INTLAB only for the optional INTLAB tests.

No MATLAB, INTLAB or Apple framework binaries/headers are included. Build products stay in ignored `build/`. Native C tests do not require MATLAB or INTLAB.

## Quick start

Clone the repository, then build from its root:

```sh
git clone https://github.com/taklab-org/INTLAB-rounding-fix.git
cd INTLAB-rounding-fix
python3 scripts/build.py
python3 scripts/test_native.py
python3 scripts/matlab.py --check
python3 scripts/matlab.py
```

The last command starts a MATLAB desktop with the patch loaded. To run a batch job:

```sh
python3 scripts/matlab.py -batch "your_function"
```

The launcher preserves the calling directory unless `-sd` is supplied. It adds the repository's MATLAB functions and build directory to `MATLABPATH`, but does not initialize INTLAB or replace your normal `startup.m`. `--check` deliberately uses an isolated test directory with a no-op startup.

MATLAB discovery prefers `/Applications/MATLAB_R2026a.app`; otherwise it accepts a single MATLAB app under `/Applications`. For a different location, use:

```sh
python3 scripts/build.py --matlab "/path/to/MATLAB.app"
```

The location is saved only in `build/config.json`. You may also set `MATLAB_ROOT`, or override the launcher with `--matlab`. Rebuild the MEX if switching MATLAB installations. Compiler/SDK discovery uses `xcrun`, with the standard Command Line Tools paths as a fallback; `build.py --cc ... --sdk ...` supports explicit locations. The scripts do not download tools or accept licenses.

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
ACCELERATE_ROUNDING_AUDIT=1 python3 scripts/matlab.py --check
```

The patch then reports intercepted BLAS calls, incoming rounding mismatches, callback thread IDs and overlapping callbacks. Counters are disabled by default; zeros with auditing disabled do not mean the library failed to load.

## Tests

Native-only build and tests:

```sh
python3 scripts/build.py --native-only
python3 scripts/test_native.py
```

The native runner compares separate unpatched and patched processes. A baseline rounding failure is recorded rather than treated as a runner failure. Patched tests must pass and must intercept callbacks on BLAS worker threads. A baseline pass on another OS is possible and is not evidence that the patch is needed there. Raw logs and machine-specific results stay in `build/`.

Build with MATLAB support, then run its tests from the repository root:

```sh
python3 scripts/build.py
python3 scripts/matlab.py -sd "$PWD/tests" -batch run_matlab_tests
```

To include the original `testmm.m`, explicitly provide a private or otherwise exclusively owned INTLAB installation:

```sh
INTLAB_ROOT="/path/to/private/Intlab" \
  python3 scripts/matlab.py -sd "$PWD/tests" -batch run_matlab_tests
```

This optional test calls `startintlab`, which can write INTLAB's cache. Do not point it at a shared runtime being initialized or used by another job. It runs `testmm(288/512/540/1024)` and a direct interval-containment check. Without `INTLAB_ROOT`, INTLAB tests are skipped explicitly. Formal proof projects should retain their own bootstrap and cache ownership rather than use this test initializer.

## How it works

On the tested system, `libBLAS.dylib` submits its parallel computation through `dispatch_apply_with_attr`. The patch interposes this call only when the immediate caller is `libBLAS.dylib`. It captures the caller's `fegetround()` result in a per-call block, sets that direction around each original callback, and restores the previous worker direction. The original iteration count, attributes and worker index are forwarded intact.

There is no global "current requested mode" shared between simultaneous products. Tests include separate callers requesting upward and downward rounding concurrently. The interposer source is in [src/rounding_interpose.c](src/rounding_interpose.c); provenance is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Scope and removal

The launcher selects `libmwAF_BLAS_ilp64.dylib` and this checkout's interposer before MATLAB starts, using a temporary launch rc. It does not edit your home startup files, launchd environment, application bundle, system libraries or signatures. It replaces the child MATLAB's `DYLD_INSERT_LIBRARIES` selection with this patch; composing other interposers is not tested. Existing MATLAB processes are unaffected.

To stop using this checkout's patch, close its MATLAB process and use your usual launcher. Any independently installed global configuration remains your own responsibility. Finder/Dock launches do not acquire this patch merely because the checkout exists. Do not set the interposer in the global launchd environment.

Limitations include the non-public dispatch ABI, other BLAS paths and data types, untested extreme values, and floating-point controls other than rounding direction. A numerical smoke test is not a certificate for a complete interval computation. Detailed measured results and version distinctions are in [docs/validation.md](docs/validation.md).

## Contributing and publication

Compatibility reports should include CPU, macOS build, MATLAB version/update, BLAS name, thread limit, tested sizes and the patch revision. An issue template is provided. Remove personal paths before posting logs.

The first release is prepared as `0.1.0`; it has not been published by these setup scripts. See [release checklist](docs/releasing.md).

Project code is provided under the [MIT License](LICENSE). External dependencies retain their own terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
