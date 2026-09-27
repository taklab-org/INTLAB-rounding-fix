#ifndef ROUNDING_POLICY_H
#define ROUNDING_POLICY_H
#include <stdint.h>

/* BLAS's private accelerator selector, observed on macOS 26.6.2.
 * SME/SME2 feature bits are published in XNU cpu_capabilities.h.
 * The caller applies this policy only to immediate libBLAS queries. */
#define ROUND_ACCELERATOR_SELECTOR UINT64_C(0x78000000)
#define ROUND_SME_FEATURES UINT64_C(0x1800000000000000)

static inline uint64_t round_blas_capabilities(uint64_t capabilities) {
    if(capabilities & ROUND_SME_FEATURES) return capabilities;
    return capabilities & ~ROUND_ACCELERATOR_SELECTOR;
}
#endif
