#include "../src/rounding_policy.h"
#include <stdio.h>

int main(void) {
    /* Exercise all selector values, with neither/either/both SME bits, and
       preserve every unrelated capability. This tests policy, not numerical
       behavior on hardware absent from the validation machine. */
    size_t cases=0;
    for(unsigned other=0;other<2;other++) {
        const uint64_t unrelated=other ? ~(ROUND_ACCELERATOR_SELECTOR | ROUND_SME_FEATURES) : 0;
        for(unsigned sme=0;sme<4;sme++) for(unsigned selector=0;selector<16;selector++) {
            const uint64_t features=(uint64_t)sme<<59;
            const uint64_t input=unrelated | features | ((uint64_t)selector<<27);
            const uint64_t expected=features ? input : unrelated;
            if(round_blas_capabilities(input)!=expected) return 1;
            cases++;
        }
    }
    printf("POLICY_COMPLETE cases=%zu passed=1\n",cases);
    return 0;
}
