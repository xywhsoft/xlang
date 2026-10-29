#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#include "../include/chardet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { xatomic32* start; int id; } worker;
static int run(void* input)
{
    worker* w = input;
    unsigned char wide[] = {0xff,0xfe,'A',0};
    const char* borrowed = xchardet_detect_name_n(wide,sizeof(wide));
    while (!xrtAtomic32Load(w->start,XMEMORY_ACQUIRE)) xrtThreadYield();
    for(int i=0;i<2000;++i) {
        assert(strcmp(xchardet_detect_name_str("hello",5),"UTF-8")==0);
        assert(strcmp(borrowed,"UTF-16LE")==0);
        assert(xchardet_detect_bom_size_n(wide,sizeof(wide))==2);
        assert(!xchardet_is_utf8_n(wide,sizeof(wide)) && xrtGetError()==NULL);
        (void)xchardet_detect_conf_n(NULL,1);
        assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
        xrtSetErrorInfo(XERR_STATE,"chardet.thread",w->id,"marker");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError())==w->id);
    }
    xrtClearError();
    return 0;
}
int main(void)
{
    xatomic32 start;
    worker workers[4];
    xthread* threads[4];
    xrtAtomic32Init(&start,0);
    for(int i=0;i<4;++i) {
        workers[i]=(worker){&start,i+1};
        threads[i]=xrtThreadCreate(run,&workers[i],0);
        assert(threads[i]);
    }
    xrtSetErrorInfo(XERR_RANGE,"chardet.thread",99,"parent");
    xrtAtomic32Store(&start,1,XMEMORY_RELEASE);
    for(int i=0;i<4;++i) {
        assert(xrtThreadWait(threads[i])==XWAIT_OK && xrtThreadExitCode(threads[i])==0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtErrorCode(xrtGetError())==99);
    xrtClearError();
    puts("chardet threads: four workers, 8000 rounds, borrowed results and TLS PASS");
    return 0;
}
