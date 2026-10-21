#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#include "../include/color_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { xatomic32 *start; int id; } worker;
static int run(void *input) {
    worker *w=input;
    char expected[XCOLOR_HEX_SIZE], current[XCOLOR_HEX_SIZE];
    assert(xcolor_hex(w->id,2,3,expected,sizeof(expected)));
    char *held=xcolor_hex_text(w->id,2,3);
    assert(held);
    while(!xrtAtomic32Load(w->start,XMEMORY_ACQUIRE)) xrtThreadYield();
    for(int i=0;i<2000;++i) {
        assert(xcolor_hex(w->id,2,3,current,sizeof(current)));
        assert(!strcmp(current,expected) && !strcmp(held,expected));
        char *result=xcolor_from_hsv_text((i%360),0.5,0.75);
        assert(result && xcolor_valid(result)); xrtFree(result);
        xrtSetErrorInfo(XERR_STATE,"color.thread",w->id,"marker");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError())==w->id);
    }
    assert(!strcmp(held,expected));
    xrtFree(held); xrtClearError();
    return 0;
}
int main(void) {
    xatomic32 start; worker workers[4]; xthread *threads[4];
    xrtAtomic32Init(&start,0);
    for(int i=0;i<4;++i) {
        workers[i]=(worker){&start,i+1};
        threads[i]=xrtThreadCreate(run,&workers[i],0); assert(threads[i]);
    }
    xrtAtomic32Store(&start,1,XMEMORY_RELEASE);
    for(int i=0;i<4;++i) {
        assert(xrtThreadWait(threads[i])==XWAIT_OK && xrtThreadExitCode(threads[i])==0);
        xrtThreadDestroy(threads[i]);
    }
    puts("color threads: four workers, 8000 rounds, result lifetime and TLS PASS");
    return 0;
}
