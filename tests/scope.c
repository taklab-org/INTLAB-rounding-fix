#define ACCELERATE_NEW_LAPACK
#define ACCELERATE_LAPACK_ILP64
#include <Accelerate/Accelerate.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#pragma STDC FENV_ACCESS ON

extern uint64_t _get_cpu_capabilities(void);
extern void dispatch_apply_with_attr(size_t,void *,void (^)(size_t,size_t));

int main(void) {
    void (*stats)(uint64_t *,size_t)=dlsym(RTLD_DEFAULT,"round_patch_stats");
    if(!stats) return 2;
    uint64_t caps=_get_cpu_capabilities();
    int n=512;
    double *a=calloc((size_t)n*n,sizeof(double));
    double *b=calloc((size_t)n*n,sizeof(double));
    double *c=calloc((size_t)n*n,sizeof(double));
    if(!a || !b || !c) return 2;
    a[0]=b[0]=1;
    if(fesetround(FE_UPWARD)) return 2;
    cblas_dgemm(CblasColMajor,CblasNoTrans,CblasNoTrans,n,n,n,1,a,n,b,n,0,c,n);
    if(fegetround()!=FE_UPWARD) return 1;
    uint64_t before[12]={0},after[12]={0}; stats(before,12);
    if(before[0]!=2 || !before[1] || !before[8]) return 1;
    /* The executable's capabilities must be unchanged even when BLAS masks. */
    if(caps!=_get_cpu_capabilities()) return 1;
    if(before[9] && !(caps & UINT64_C(0x78000000))) return 1;
    __block size_t visits=0;
    /* n=1 runs inline. Mutate rounding inside the callbacks: a wrongly scoped
       wrapper would restore it, making this check fail. */
    dispatch_apply(1,dispatch_get_global_queue(0,0),^(size_t i) {
        (void)i; visits++; if(fesetround(FE_DOWNWARD)) abort();
    });
    if(fegetround()!=FE_DOWNWARD) return 1;
    dispatch_apply_with_attr(1,NULL,^(size_t i,size_t worker) {
        (void)i; (void)worker; visits++; if(fesetround(FE_TONEAREST)) abort();
    });
    if(fegetround()!=FE_TONEAREST || visits!=2) return 1;
    stats(after,12);
    for(size_t i=0;i<12;i++) if(before[i]!=after[i]) return 1;
    free(a); free(b); free(c);
    puts("SCOPE_COMPLETE passed=1");
    return 0;
}
