#include "mex.h"
#include <dlfcn.h>
#include <fenv.h>
#include <stdint.h>

/* No MATLAB/Apple/INTLAB implementation is bundled. */
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    if (nrhs == 1 && nlhs == 0) {
        if (!mxIsDouble(prhs[0]) || mxIsComplex(prhs[0]) || mxGetNumberOfElements(prhs[0]) != 1)
            mexErrMsgIdAndTxt("roundPatch:args", "Expected one rounding mode scalar.");
        double value = mxGetScalar(prhs[0]);
        if (value != FE_TONEAREST && value != FE_DOWNWARD && value != FE_UPWARD && value != FE_TOWARDZERO)
            mexErrMsgIdAndTxt("roundPatch:args", "Invalid rounding mode.");
        if (fesetround((int)value)) mexErrMsgIdAndTxt("roundPatch:round", "Could not restore rounding mode.");
        return;
    }
    if (nrhs || nlhs != 1)
        mexErrMsgIdAndTxt("roundPatch:args", "Use s=rounding_patch_status().");
    void (*get)(uint64_t *, size_t) = dlsym(RTLD_DEFAULT, "round_patch_stats");
    if (!get) mexErrMsgIdAndTxt("roundPatch:notLoaded", "Patch not loaded. Start a fresh MATLAB using scripts/matlab.sh.");
    uint64_t stats[12] = {0}; get(stats, 12);
    const char *fields[] = {"abi", "auditEnabled", "stats", "rounding", "library",
        "cpuFallbackSelected", "capabilityQueries", "attributeApplyCalls", "applyCalls"};
    plhs[0] = mxCreateStructMatrix(1, 1, 9, fields);
    mxSetField(plhs[0], 0, "abi", mxCreateDoubleScalar((double)stats[0]));
    mxSetField(plhs[0], 0, "auditEnabled", mxCreateLogicalScalar(stats[1] != 0));
    mxArray *array = mxCreateDoubleMatrix(1, 12, mxREAL);
    double *values = mxGetPr(array);
    for (int i = 0; i < 12; i++) values[i] = (double)stats[i];
    mxSetField(plhs[0], 0, "stats", array);
    mxSetField(plhs[0], 0, "rounding", mxCreateDoubleScalar(fegetround()));
    mxSetField(plhs[0], 0, "cpuFallbackSelected", mxCreateLogicalScalar(stats[9] != 0));
    mxSetField(plhs[0], 0, "capabilityQueries", mxCreateDoubleScalar((double)stats[8]));
    mxSetField(plhs[0], 0, "attributeApplyCalls", mxCreateDoubleScalar((double)stats[10]));
    mxSetField(plhs[0], 0, "applyCalls", mxCreateDoubleScalar((double)stats[11]));
    Dl_info info;
    mxSetField(plhs[0], 0, "library", mxCreateString(dladdr((void *)get, &info) ? info.dli_fname : "unknown"));
}
