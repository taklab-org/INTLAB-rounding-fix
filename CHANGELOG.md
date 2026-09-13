# Changelog

## 0.1.0 — prepared, not yet released

- Forward the calling thread's rounding direction to Apple Accelerate's internal BLAS callbacks, preserving its partitioning and parallel execution.
- Provide local build discovery, an opt-in MATLAB launcher, a loaded-patch check, and independent native and MATLAB enclosure tests.
- Use macOS’s bundled Bash for build, launch and test scripts; no Python runtime dependency. The Accelerate interposer remains C, with optional MATLAB checks.
- Include original INTLAB `testmm` and a benchmark summary from the M4 Pro validation.
