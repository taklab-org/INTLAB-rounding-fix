#define ACCELERATE_NEW_LAPACK
#define ACCELERATE_LAPACK_ILP64
#include <Accelerate/Accelerate.h>
#include <time.h>
#include <fenv.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#pragma STDC FENV_ACCESS ON
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static int compare(const void *a,const void *b) {double x=*(double*)a,y=*(double*)b;return (x>y)-(x<y);}
int main(int argc,char **argv) {
    int single=argc>1?atoi(argv[1]):0;
    BLASSetThreading(single?BLAS_THREADING_SINGLE_THREADED:BLAS_THREADING_MULTI_THREADED);
    int sizes[]={512,1024,2048,4096};
    for(int s=0;s<4;s++) {
        int n=sizes[s], reps=n<=1024?50:(n==2048?15:4);
        size_t len=(size_t)n*n; double *a=malloc(len*8),*b=malloc(len*8),*c=malloc(len*8);
        if(!a||!b||!c) abort();
        for(size_t i=0;i<len;i++){a[i]=((int)(i%101)-50)*0x1p-7;b[i]=((int)(i%103)-51)*0x1p-7;}
        for(int mode=0;mode<2;mode++) {
            fesetround(mode?FE_UPWARD:FE_TONEAREST);
            for(int warm=0;warm<3;warm++) cblas_dgemm(CblasColMajor,CblasNoTrans,CblasNoTrans,n,n,n,1,a,n,b,n,0,c,n);
            double times[7];
            for(int r=0;r<7;r++) {
                double t=now();
                for(int i=0;i<reps;i++) cblas_dgemm(CblasColMajor,CblasNoTrans,CblasNoTrans,n,n,n,1,a,n,b,n,0,c,n);
                times[r]=(now()-t)/reps;
            }
            fesetround(FE_TONEAREST);qsort(times,7,sizeof(double),compare);
            printf("BENCH single=%d n=%d rounding=%s median_ms=%.6f min_ms=%.6f max_ms=%.6f checksum=%a\n",single,n,mode?"up":"nearest",times[3]*1000,times[0]*1000,times[6]*1000,c[len/2]); fflush(stdout);
        }
        free(a);free(b);free(c);
    }
}
