#define ACCELERATE_NEW_LAPACK
#define ACCELERATE_LAPACK_ILP64
#include <Accelerate/Accelerate.h>
#include <fenv.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#pragma STDC FENV_ACCESS ON
static uint64_t step(uint64_t *s) { *s^=*s<<13; *s^=*s>>7; *s^=*s<<17; return *s; }
static int64_t number(uint64_t *s) {
    uint64_t x=step(s); int64_t v=(INT64_C(1)<<52)+(int64_t)(x>>12);
    return x&1 ? v : -v;
}
static int check(int m,int n,int k,int ta,int tb,int mode,uint64_t seed) {
    size_t na=(size_t)m*k, nb=(size_t)k*n;
    double *a=malloc(na*8),*b=malloc(nb*8),*c=malloc((size_t)m*n*8);
    int64_t *ia=malloc(na*8),*ib=malloc(nb*8);
    if(!a||!b||!c||!ia||!ib) abort();
    for(size_t i=0;i<na;i++) {ia[i]=number(&seed);a[i]=ldexp((double)ia[i],-52);}
    for(size_t i=0;i<nb;i++) {ib[i]=number(&seed);b[i]=ldexp((double)ib[i],-52);}
    BLASSetThreading(BLAS_THREADING_MULTI_THREADED);
    if(fesetround(mode)) abort();
    cblas_dgemm(CblasColMajor,ta?CblasTrans:CblasNoTrans,tb?CblasTrans:CblasNoTrans,m,n,k,1,a,ta?k:m,b,tb?n:k,0,c,m);
    int bad=(fegetround()!=mode); fesetround(FE_TONEAREST);
    /* Exact integer oracle: inputs ia/2^52 and ib/2^52, sum in int128.
       Bounds fit in 120 bits. Scaling these outputs by 2^104 is integral. */
    for(int t=0;t<64;t++) {
        int row=(int)(step(&seed)%(uint64_t)m),col=(int)(step(&seed)%(uint64_t)n);
        __int128 exact=0;
        for(int q=0;q<k;q++) {
            size_t ai=ta ? q+(size_t)row*k : row+(size_t)q*m;
            size_t bi=tb ? col+(size_t)q*n : q+(size_t)col*k;
            exact+=(__int128)ia[ai]*ib[bi];
        }
        double value=c[row+(size_t)col*m];
        /* Reject out-of-range results before converting a scaled value to
           int128, so even a broken BLAS cannot cause conversion overflow. */
        if(!isfinite(value) || fabs(value)>16.0*k) {bad++;continue;}
        __int128 actual=(__int128)ldexp(value,104);
        if(mode==FE_DOWNWARD) bad+=actual>exact;
        else bad+=actual<exact;
    }
    free(a);free(b);free(c);free(ia);free(ib); return bad;
}
typedef struct {int mode,bad;} Job;
static void *parallel(void *arg) {
    Job *j=arg;
    for(int rep=0;rep<20;rep++) j->bad+=check(513,540,289,rep%2,(rep/2)%2,j->mode,100+(uint64_t)rep);
    return NULL;
}
int main(void) {
    int shapes[][3]={{288,288,288},{513,513,513},{1024,1024,1024},{383,541,289}};
    int modes[]={FE_DOWNWARD,FE_UPWARD}; int bad=0,cases=0;
    for(int s=0;s<4;s++) for(int ta=0;ta<2;ta++) for(int tb=0;tb<2;tb++) for(int r=0;r<2;r++) {
        bad+=check(shapes[s][0],shapes[s][1],shapes[s][2],ta,tb,modes[r],1+(uint64_t)cases); cases++;
    }
    printf("STRESS serial_calls=%d sampled_outputs=%d violations=%d\n",cases,cases*64,bad);
    pthread_t t[2]; Job jobs[2]={{FE_DOWNWARD,0},{FE_UPWARD,0}};
    for(int i=0;i<2;i++) if(pthread_create(&t[i],NULL,parallel,&jobs[i])) abort();
    for(int i=0;i<2;i++) {pthread_join(t[i],NULL);bad+=jobs[i].bad;}
    printf("STRESS concurrent_calls=40 sampled_outputs=2560 down_violations=%d up_violations=%d\n",jobs[0].bad,jobs[1].bad);
    printf("STRESS_COMPLETE violations=%d\n",bad); return bad?1:0;
}
