/* 独立测公开API；不把单个负载的结果外推为整个库提速。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TIME
#include "xrt.h"
#include "../include/diff_native.h"
#ifdef __TINYC__
#include "../diff_native.c"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned rounds=(unsigned)strtoul(argv[2],NULL,10);
    if(!rounds || rounds>10000)return 2;
    const unsigned count=1024;char *left=xrtMalloc(count*16+1),*right=xrtMalloc(count*16+1);
    if(!left || !right){xrtFree(left);xrtFree(right);return 2;}
    size_t used=0;for(unsigned i=0;i<count;++i)used+=(size_t)sprintf(left+used,"row-%04u\n",i);
    memcpy(right,left,used+1);
    if(!strcmp(argv[1],"early"))right[0]='X';
    else if(!strcmp(argv[1],"late"))right[used-2]='X';
    else if(strcmp(argv[1],"same")){xrtFree(left);xrtFree(right);return 2;}
    int code=1;char* warm=xdiff_stats(left,right);if(!warm)goto done;xrtFree(warm);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    double start=xrtTimer();unsigned long long bytes=0;
    for(unsigned i=0;i<rounds;++i){
        char* result=xdiff_stats(left,right);if(!result || xrtGetError()){xrtFree(result);goto done;}
        bytes+=strlen(result);xrtFree(result);
    }
    double stop=xrtTimer();xrtMemDebugSnapshot(&after);
    if(!isfinite(start) || !isfinite(stop) || stop<start)goto done;
    printf("diff bench: %s rounds=%u seconds=%.9f bytes=%llu alloc=%llu\n",argv[1],rounds,
        stop-start,bytes,(unsigned long long)(after.AllocCount-before.AllocCount));code=0;
done:
    xrtFree(left);xrtFree(right);return code;
}
