/* Experimental macOS arm64 patch, validated on macOS 26.6.2.
 * Interpose ONLY libBLAS callers of dispatch_apply_with_attr.
 * Preserve Accelerate's iterations, attributes, worker indices and kernels.
 * This SPI is exported by libdispatch but absent from the public SDK header.
 * Signature source: apple-oss-distributions/libdispatch/src/apply.c.
 */
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <fenv.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma STDC FENV_ACCESS ON

typedef struct dispatch_apply_attr_s *round_apply_attr_t;
extern void dispatch_apply_with_attr(size_t,round_apply_attr_t,void (^)(size_t,size_t));
static int audit;
static _Atomic uint64_t calls, callbacks, mismatches, foreign, active, peak;
static _Atomic uint64_t tids[64];

static int from_blas(const void *address) {
    Dl_info info;
    if(!dladdr(address,&info) || !info.dli_fname) return 0;
    const char *base=strrchr(info.dli_fname,'/');
    return base && strcmp(base+1,"libBLAS.dylib")==0;
}
static void observe(int saved,int requested,pthread_t caller) {
    atomic_fetch_add_explicit(&callbacks,1,memory_order_relaxed);
    if(saved!=requested) atomic_fetch_add_explicit(&mismatches,1,memory_order_relaxed);
    if(!pthread_equal(pthread_self(),caller)) atomic_fetch_add_explicit(&foreign,1,memory_order_relaxed);
    uint64_t tid=0; pthread_threadid_np(NULL,&tid);
    for(size_t i=0;i<64;i++) {
        uint64_t expected=0;
        if(atomic_load_explicit(&tids[i],memory_order_relaxed)==tid ||
           atomic_compare_exchange_strong_explicit(&tids[i],&expected,tid,memory_order_relaxed,memory_order_relaxed)) break;
    }
    uint64_t current=atomic_fetch_add_explicit(&active,1,memory_order_relaxed)+1;
    uint64_t previous=atomic_load_explicit(&peak,memory_order_relaxed);
    while(current>previous && !atomic_compare_exchange_weak_explicit(&peak,&previous,current,memory_order_relaxed,memory_order_relaxed)) {}
}
__attribute__((noinline))
static void patched_apply_attr(size_t n,round_apply_attr_t attr,void (^fn)(size_t,size_t)) {
    if(!from_blas(__builtin_return_address(0))) {
        dispatch_apply_with_attr(n,attr,fn);
        return;
    }
    const int requested=fegetround();
    const pthread_t caller=pthread_self();
    if(audit) atomic_fetch_add_explicit(&calls,1,memory_order_relaxed);
    dispatch_apply_with_attr(n,attr,^(size_t i,size_t worker) {
        const int saved=fegetround();
        if(audit) observe(saved,requested,caller);
        if(fesetround(requested)) abort();
        fn(i,worker);
        if(fesetround(saved)) abort();
        if(audit) atomic_fetch_sub_explicit(&active,1,memory_order_relaxed);
    });
}
__attribute__((used)) static const struct {const void *replacement,*original;}
round_interpose __attribute__((section("__DATA,__interpose")))={
    (const void *)&patched_apply_attr,(const void *)&dispatch_apply_with_attr
};
/* Snapshot diagnostics; counters are collected only with the audit env flag.
 * [ABI version, audit, calls, callbacks, incoming mismatches, foreign callbacks,
 *  peak overlapping callbacks, unique callback thread IDs (capped at 64)]. */
void round_patch_stats(uint64_t *out,size_t count) {
    uint64_t unique=0;
    for(size_t i=0;i<64;i++) unique+=atomic_load_explicit(&tids[i],memory_order_relaxed)!=0;
    uint64_t data[8]={1,(uint64_t)audit,calls,callbacks,mismatches,foreign,peak,unique};
    for(size_t i=0;i<count && i<8;i++) out[i]=data[i];
}
__attribute__((constructor)) static void initialize(void) {
    const char *flag=getenv("ACCELERATE_ROUNDING_AUDIT");
    audit=flag && strcmp(flag,"1")==0;
}
__attribute__((destructor)) static void report(void) {
    if(!audit) return;
    uint64_t s[8]; round_patch_stats(s,8);
    fprintf(stderr,"ROUND_PATCH ABI=1 blas_calls=%llu callbacks=%llu incoming_round_mismatch=%llu foreign_thread_callbacks=%llu peak_overlapping_callbacks=%llu unique_callback_threads=%llu\n",
       (unsigned long long)s[2],(unsigned long long)s[3],(unsigned long long)s[4],(unsigned long long)s[5],(unsigned long long)s[6],(unsigned long long)s[7]);
}
