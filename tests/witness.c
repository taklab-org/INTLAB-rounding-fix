#define ACCELERATE_NEW_LAPACK
#define ACCELERATE_LAPACK_ILP64
#include <Accelerate/Accelerate.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#pragma STDC FENV_ACCESS ON

int main(void) {
    const int sizes[]={2,7,8,16,17,31,64,127,128,255,256,287,288,512,513,540,1024,2048};
    size_t total=0;
    if(BLASSetThreading(BLAS_THREADING_MULTI_THREADED)) return 2;
    for(size_t s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++) {
        int n=sizes[s]; size_t length=(size_t)n*n;
        double *a=calloc(length,sizeof(double)), *b=calloc(length,sizeof(double)), *c=calloc(length,sizeof(double));
        if(!a||!b||!c) return 2;
        for(int i=0;i<n;i++){a[i]=1;a[i+n]=0x1p-27;b[(size_t)i*n]=1;b[(size_t)i*n+1]=0x1p-27;}
        for(int sign=1;sign>=-1;sign-=2) {
            for(int i=0;i<n;i++){a[i]=sign;a[i+n]=sign*0x1p-27;}
            for(int up=0;up<2;up++) {
                if(fesetround(up?FE_UPWARD:FE_DOWNWARD)) return 2;
                cblas_dgemm(CblasColMajor,CblasNoTrans,CblasNoTrans,n,n,n,1,a,n,b,n,0,c,n);
                if(fegetround()!=(up?FE_UPWARD:FE_DOWNWARD)) return 2;
                fesetround(FE_TONEAREST);
                double lower=sign>0?1:-0x1.0000000000001p0;
                double upper=sign>0?0x1.0000000000001p0:-1;
                size_t bad=0;
                for(size_t i=0;i<length;i++) bad+=!isfinite(c[i]) || (up?c[i]<upper:c[i]>lower);
                total+=bad;
                printf("WITNESS n=%d sign=%d up=%d bad=%zu\n",n,sign,up,bad);
            }
        }
        free(a);free(b);free(c);
    }
    printf("WITNESS_COMPLETE bad=%zu\n",total);
    return total?1:0;
}
