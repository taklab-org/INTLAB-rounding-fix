# Changelog

## Unreleased — patch ABI 2

- Repair the tested M3 Max path by selecting Accelerate CPU kernels through a BLAS-only capability filter on non-SME hardware.
- Propagate rounding through `dispatch_apply` as well as `dispatch_apply_with_attr`, preserving native BLAS parallel execution.
- Select the path automatically from CPU capabilities; add 128 selection-policy cases and reject stale ABI 1 build metadata at launch.
- Expose CPU-path selection and per-API counters in the status MEX. Rebuild the dylib and MEX together.
- Add dispatch/capability scope regression checks and expand exact real-double containment tests.
- Record M3 Max / R2026a Update 2 / INTLAB V13 results, including fresh process workers. M4 revalidation remains outstanding.

## 0.1.0 — prepared, not yet released

- Forward the calling thread's rounding direction to Apple Accelerate's internal BLAS callbacks, preserving its partitioning and parallel execution.
- Provide local build discovery, an opt-in MATLAB launcher, a loaded-patch check, and independent native and MATLAB enclosure tests.
- Use macOS’s bundled Bash for build, launch and test scripts; no Python runtime dependency. The Accelerate interposer remains C, with optional MATLAB checks.
- Include original INTLAB `testmm` and a benchmark summary from the M4 Pro validation.
