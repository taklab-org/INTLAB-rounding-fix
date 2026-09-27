/* Experimental macOS arm64 patch, validated on macOS 26.6.2.
 * Interpose ONLY immediate libBLAS callers. On non-SME hardware, select
 * Accelerate's CPU kernels and propagate rounding through both apply APIs.
 * Use Accelerate's own kernels; forward iterations, queues and attributes.
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
#include "rounding_policy.h"
#pragma STDC FENV_ACCESS ON

typedef struct dispatch_apply_attr_s *round_apply_attr_t;
extern void dispatch_apply_with_attr(size_t,round_apply_attr_t,void (^)(size_t,size_t));
static int audit;
static _Atomic uint64_t calls, callbacks, mismatches, foreign, active, peak;
static _Atomic uint64_t tids[64];
static _Atomic uint64_t attr_calls, apply_calls, caps_calls, caps_masked;

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
    if(audit) {
        atomic_fetch_add_explicit(&calls,1,memory_order_relaxed);
        atomic_fetch_add_explicit(&attr_calls,1,memory_order_relaxed);
    }
    dispatch_apply_with_attr(n,attr,^(size_t i,size_t worker) {
        const int saved=fegetround();
        if(audit) observe(saved,requested,caller);
        if(fesetround(requested)) abort();
        fn(i,worker);
        if(fesetround(saved)) abort();
        if(audit) atomic_fetch_sub_explicit(&active,1,memory_order_relaxed);
    });
}
/* Snapshot diagnostics; callback counters require the audit env flag.
 * [ABI version, audit, calls, callbacks, incoming mismatches, foreign callbacks,
 *  peak overlapping callbacks, unique callback thread IDs (capped at 64), capability queries, masked
 *  capability queries, attribute apply calls, ordinary apply calls].
 * Capability counts are always collected (initialization only). */
void round_patch_stats(uint64_t *out,size_t count) {
    uint64_t unique=0;
    for(size_t i=0;i<64;i++) unique+=atomic_load_explicit(&tids[i],memory_order_relaxed)!=0;
    uint64_t data[12]={2,(uint64_t)audit,calls,callbacks,mismatches,foreign,peak,unique,
        caps_calls,caps_masked,attr_calls,apply_calls};
    for(size_t i=0;i<count && i<12;i++) out[i]=data[i];
}
__attribute__((constructor)) static void initialize(void) {
    const char *flag=getenv("ACCELERATE_ROUNDING_AUDIT");
    audit=flag && strcmp(flag,"1")==0;
}
__attribute__((destructor)) static void report(void) {
    if(!audit) return;
    uint64_t s[12]; round_patch_stats(s,12);
    fprintf(stderr,"ROUND_PATCH ABI=2 blas_calls=%llu callbacks=%llu incoming_round_mismatch=%llu foreign_thread_callbacks=%llu peak_overlapping_callbacks=%llu unique_callback_threads=%llu caps_queries=%llu caps_masked=%llu attr_calls=%llu apply_calls=%llu\n",
       (unsigned long long)s[2],(unsigned long long)s[3],(unsigned long long)s[4],(unsigned long long)s[5],(unsigned long long)s[6],(unsigned long long)s[7],(unsigned long long)s[8],(unsigned long long)s[9],
       (unsigned long long)s[10],(unsigned long long)s[11]);
}

__attribute__((noinline))
static void patched_apply(size_t n,dispatch_queue_t queue,void (^fn)(size_t)) {
    if(!from_blas(__builtin_return_address(0))) {
        dispatch_apply(n,queue,fn);
        return;
    }
    const int requested=fegetround();
    const pthread_t caller=pthread_self();
    if(audit) {
        atomic_fetch_add_explicit(&calls,1,memory_order_relaxed);
        atomic_fetch_add_explicit(&apply_calls,1,memory_order_relaxed);
    }
    dispatch_apply(n,queue,^(size_t i) {
        const int saved=fegetround();
        if(audit) observe(saved,requested,caller);
        if(fesetround(requested)) abort();
        fn(i);
        if(fesetround(saved)) abort();
        if(audit) atomic_fetch_sub_explicit(&active,1,memory_order_relaxed);
    });
}
/* _get_cpu_capabilities is Apple-internal SPI (xnu cpu_capabilities.h).
 * The selector in bits 27..30 is NOT documented by that public source.
 * Its use was observed in libBLAS on macOS 26.6.2 (25G83): initialization
 * extracts this nibble to select accelerator kernels. Clearing it selects
 * the framework's CPU path; do not change the OS commpage or other callers.
 * SME/SME2 bits 59/60 are documented by XNU. Preserve the M4 SME path and
 * its original partitioning. No per-product global policy changes: BLAS
 * caches this choice once, including for round-to-nearest products.
 * Revalidate both this SPI and numerical containment after OS changes.
 */
extern uint64_t _get_cpu_capabilities(void);
__attribute__((noinline))
static uint64_t patched_caps(void) {
    uint64_t caps=_get_cpu_capabilities();
    if(from_blas(__builtin_return_address(0))) {
        atomic_fetch_add_explicit(&caps_calls,1,memory_order_relaxed);
        const uint64_t selected=round_blas_capabilities(caps);
        if(selected!=caps) {
            atomic_fetch_add_explicit(&caps_masked,1,memory_order_relaxed);
        }
        return selected;
    }
    return caps;
}
__attribute__((used)) static const struct {const void *replacement,*original;}
round_interpose[] __attribute__((section("__DATA,__interpose")))={
    {(const void *)&patched_apply_attr,(const void *)&dispatch_apply_with_attr},
    {(const void *)&patched_apply,(const void *)&dispatch_apply},
    {(const void *)&patched_caps,(const void *)&_get_cpu_capabilities}
};
