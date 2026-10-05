#include "src/runtime/xl6_future_metadata.h"
#include "src/api/xl_future_bridge.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}
static void expectText(xstrview text,xstrview expected)
{
    assert(text.Data && text.Size==expected.Size && !memcmp(text.Data,expected.Data,text.Size));
    xrtFree((void*)text.Data);
}
static void functional(void)
{
    for(unsigned i=0;i<1000;++i) {
        xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
        assert(xl6FutureDebugNameSetV2(future,VIEW("exact\0你")));
        xstrview owned=xl6FutureDebugNameCopyV2(future);
        assert(xl6FutureDebugNameSetV2(future,VIEW("changed")));
        expectText(xl6FutureDebugNameCopyV2(future),VIEW("changed"));
        int64_t first=xl6FutureFirstObservedMsV2(future);
        assert(!xrtGetError() && first>=0 && xl6FutureFirstTerminalObservedMsV2(future)==0 &&
            xl6FuturePendingWatchesV2(future)==0);
        assert(xrtPromiseResolve(promise,NULL));
        assert(xl6FutureFirstTerminalObservedMsV2(future)>=first);
        xrtPromiseDestroy(promise); xrtFutureDestroy(future);
        expectText(owned,VIEW("exact\0你"));
    }
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    xrtSetErrorKind(XERR_PROTOCOL);
    assert(xl6FutureDebugNameSetV2(future,VIEW("valid")) && !xrtGetError());
    assert(!xl6FutureDebugNameSetV2(future,(xstrview){"\xff",1}) && xrtErrorKind(xrtGetError())==XERR_TYPE);
    expectText(xl6FutureDebugNameCopyV2(future),VIEW("valid"));
    assert(xrtFutureDebugSetNameN(future,"\xff",1));
    assert(!xl6FutureDebugNameCopyV2(future).Data && xrtErrorKind(xrtGetError())==XERR_TYPE);
    assert(xl6FutureDebugNameSetV2(future,(xstrview){NULL,0}));
    expectText(xl6FutureDebugNameCopyV2(future),VIEW(""));
    assert(!xl6FutureDebugNameSetV2(NULL,VIEW("name")) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError(); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
typedef struct Gate { xmutex lock; xcond condition; bool entered,proceed; xthread* thread; } Gate;
static int accepted(void* data,xl6_future_result* result)
{
    Gate* gate=data; assert(xrtMutexLock(&gate->lock));
    gate->thread=xrtThreadRef(xrtThreadCurrent()); assert(gate->thread);
    gate->entered=true; assert(xrtCondSignal(&gate->condition));
    while(!gate->proceed) assert(xrtCondWaitFor(&gate->condition,&gate->lock,UINT64_C(10000000))==XWAIT_OK);
    assert(xrtMutexUnlock(&gate->lock));
    result->iStatus=XL6_FUTURE_STATUS_OK; return 0;
}
static void cancellation(void)
{
    for(unsigned i=0;i<100;++i) {
        Gate gate={0}; assert(xrtMutexInit(&gate.lock) && xrtCondInit(&gate.condition));
        xfuture* future=xl6FutureRunThread(accepted,&gate,0); assert(future);
        assert(xrtMutexLock(&gate.lock));
        while(!gate.entered) assert(xrtCondWaitFor(&gate.condition,&gate.lock,UINT64_C(10000000))==XWAIT_OK);
        assert(xl6FutureDebugNameSetV2(future,VIEW("accepted\0task")));
        assert(xl6FutureCancel(future) && xrtFutureState(future)==XFUTURE_PENDING);
        gate.proceed=true; assert(xrtCondSignal(&gate.condition) && xrtMutexUnlock(&gate.lock));
        assert(xrtThreadWaitFor(gate.thread,UINT64_C(10000000))==XWAIT_OK);
        assert(xrtFutureState(future)==XFUTURE_CANCELLED);
        xl6FutureDestroy(future); xrtThreadDestroy(gate.thread);
        assert(xrtCondUnit(&gate.condition) && xrtMutexUnit(&gate.lock));
    }
}
static void sweep(unsigned mode,size_t point)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    if(mode>=5) assert(xl6FutureDebugNameSetV2(future,VIEW("original")));
    xstrview owned={0}; bool ok;
    assert(xrtMemDebugFailAfter(point));
    if(mode==0 || mode==5) ok=xl6FutureDebugNameSetV2(future,VIEW("new\0你"));
    else if(mode==1 || mode==6) { owned=xl6FutureDebugNameCopyV2(future); ok=owned.Data!=NULL; }
    else {
        if(mode==2) (void)xl6FutureFirstObservedMsV2(future);
        if(mode==3) (void)xl6FutureFirstTerminalObservedMsV2(future);
        if(mode==4) (void)xl6FuturePendingWatchesV2(future);
        ok=xrtGetError()==NULL;
    }
    bool hit=xl6ProductMemoryEnd(); assert(ok==!hit);
    if(!ok) assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);
    xrtClearError(); xrtFree((void*)owned.Data);
    if(!ok) expectText(xl6FutureDebugNameCopyV2(future),mode>=5 ? VIEW("original") : VIEW(""));
    assert(xrtFutureState(future)==XFUTURE_PENDING);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); xrtClearError(); xl6ProductMemoryVerify();
    printf("metadata mode=%u point=%zu hit=%u balanced\n",mode,point,(unsigned)hit);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==3) { unsigned mode=(unsigned)strtoul(argv[1],NULL,10); assert(mode<7);
        sweep(mode,(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); functional(); cancellation(); assert(!xrtGetError());
    (void)xl6ProductMemoryEnd(); xl6ProductMemoryVerify();
    puts("Future metadata: 1000 owned exact-name lifetimes, UTF-8 boundaries, 100 cooperative accepted tasks balanced");
    return 0;
}
