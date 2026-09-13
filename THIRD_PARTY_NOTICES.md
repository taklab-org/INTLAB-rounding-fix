# Sources and external dependencies

The rounding interposer is project code. Its private dispatch function declaration was checked against Apple's published implementation:

- Apple libdispatch: https://github.com/apple-oss-distributions/libdispatch/blob/main/src/apply.c
- Apple dyld interposition model: https://github.com/apple-oss-distributions/dyld/blob/main/dyld/DyldRuntimeState.h

Apple's implementations are not copied or bundled. The build uses headers and libraries from the user's installed Apple development tools and frameworks. `dispatch_apply_with_attr` is exported by the tested OS but absent from its public SDK headers.

MATLAB, its headers and libraries, and INTLAB are external dependencies. They are not distributed or relicensed by this repository. Native tests need neither MATLAB nor INTLAB. The MATLAB status MEX is compiled against the user's own MATLAB installation. INTLAB tests require a separately installed INTLAB runtime.

`tests/testmm.m` is the original matrix-rounding test supplied by the project author, copied without changes. Other test programs and launch tools were developed for this investigation.

This project is not an official Apple, MathWorks or INTLAB release and is not affiliated with or endorsed by those projects. Product names identify the software tested.
