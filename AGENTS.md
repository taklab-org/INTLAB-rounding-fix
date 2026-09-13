# Project scope

This repository contains a macOS arm64 rounding-mode interposer for Apple Accelerate, launch tools and independent tests. Read README.md and docs/validation.md before changing numerical behavior.

- Preserve Accelerate's internal parallelism and the BLAS-only interposition scope. Do not substitute single-threaded multiplication to make tests pass.
- Keep machine-specific configuration, build products and raw local logs under ignored build/.
- Do not bundle MATLAB, INTLAB, Apple framework binaries or their headers.
- Do not modify global startup files, OS libraries or app signatures.
- Validate source changes with the native exact-oracle tests and, when available, MATLAB tests. Separate verified results from user-reported compatibility.
- Public deployment is separate from local preparation; do not push or publish a release unless requested.
