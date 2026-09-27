# Sources and external dependencies

The rounding interposer is project code. Its private dispatch function declaration was checked against Apple's published implementation:

- Apple libdispatch: https://github.com/apple-oss-distributions/libdispatch/blob/main/src/apply.c
- Apple XNU capability SPI declaration and SME feature bits: https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/arm/cpu_capabilities.h
- Apple dyld interposition model: https://github.com/apple-oss-distributions/dyld/blob/main/dyld/DyldRuntimeState.h

The capability selector at bits 27–30 is not documented in XNU's published header. Its role was inferred from the locally installed libBLAS initialization on macOS 26.6.2 and tested by numerical ablation; it is not a public API guarantee. Apple's implementations, disassembly and headers are not copied or bundled in the source distribution. The build uses headers and libraries from the user's installed Apple development tools and frameworks. `dispatch_apply_with_attr` is exported by the tested OS but absent from its public SDK headers.

MATLAB, its headers and libraries, and INTLAB are external dependencies. They are not distributed or relicensed by this repository. Native tests need neither MATLAB nor INTLAB. The MATLAB status MEX is compiled against the user's own MATLAB installation. INTLAB tests require a separately installed INTLAB runtime.

`tests/testmm.m` is the original matrix-rounding test supplied by the project author, copied without changes. Other test programs and launch tools were developed for this investigation.

This project is not an official Apple, MathWorks or INTLAB release and is not affiliated with or endorsed by those projects. Product names identify the software tested.
