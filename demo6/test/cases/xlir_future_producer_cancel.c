/* Actual Future core, not a substitute cancellation implementation. The same
 * fixture can compile against the narrowly backported upstream single header. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#define XRT_IMPLEMENTATION
#ifndef XLIR_PRODUCER_FUTURE_HEADER
#define XLIR_PRODUCER_FUTURE_HEADER "lib/xrt/xrt.h"
#endif
#include XLIR_PRODUCER_FUTURE_HEADER
#include <assert.h>
#include <stdio.h>

typedef struct Producer {
    xatomic32 refs, retains, drops, requests, entered, release;
    xfuture* future; /* borrowed identity; each calling thread owns its endpoint */
    xpromise* promise; /* borrowed; test owns the actual producer endpoint */
    bool refuse;
    unsigned mode;
} Producer;
static int payload=31;
static uint32 atomicCount(const xatomic32* value) {return xrtAtomic32Load(value,XMEMORY_ACQUIRE);}
static bool count(const void* data,size_t* result)
{const Producer* p=data;*result=atomicCount(&p->refs);return *result!=0;}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)context;return visit!=NULL;} /* no owned child slots */
static const xrtownershipops ops={count,trace};
static bool retain(const void* data)
{
    Producer* p=(Producer*)data;
    if(p->refuse)return false;
    uint32 previous=xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_ACQ_REL);
    assert(previous>0&&previous<UINT32_MAX);
    xrtAtomic32FetchAdd(&p->retains,1,XMEMORY_RELAXED);return true;
}
static void drop(const void* data)
{
    Producer* p=(Producer*)data;
    assert(xrtAtomic32FetchAdd(&p->refs,UINT32_MAX,XMEMORY_ACQ_REL)>0);
    xrtAtomic32FetchAdd(&p->drops,1,XMEMORY_RELAXED);
}
static int32 scopeProbe(void* data)
{
    Producer* p=data;xrtownershipscope freeze={0};bool acquired=false;
    for(unsigned i=0;i<1000000&&!(acquired=xrtOwnershipFreezeTryBegin(&freeze));++i)xrtThreadYield();
    assert(acquired&&xrtFutureState(p->future)==XFUTURE_PENDING);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void request(const void* data)
{
    Producer* p=(Producer*)data;
    assert(atomicCount(&p->refs)>=2);
    assert(xrtAtomic32FetchAdd(&p->requests,1,XMEMORY_ACQ_REL)==0);
    if(p->mode==1){
        /* Terminal publication returns the Future's producer edge while this
         * call is still live. The acquired cancellation-tail reference wins. */
        assert(xrtPromiseResolve(p->promise,&payload));
        assert(atomicCount(&p->refs)==1&&!xrtFutureCancel(p->future));
    }else if(p->mode==2){
        xrtAtomic32Store(&p->entered,1,XMEMORY_RELEASE);
        while(!atomicCount(&p->release))xrtThreadYield();
        assert(atomicCount(&p->refs)==1&&xrtFutureState(p->future)==XFUTURE_RESOLVED);
    }else if(p->mode==3){
        /* This real second thread needs BOTH the ownership freeze and Future
         * lock before Request returns. A callback held under either deadlocks. */
        xthread* thread=xrtThreadCreate(scopeProbe,p,0);assert(thread);
        assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);
        assert(xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    }
}
static const xfutureproducerownershipv1 policy={sizeof(policy),drop};
static const xfutureproducercancellationv1 cancellation={sizeof(cancellation),retain,request};
static void start(Producer* p,unsigned mode,bool legacy)
{
    *p=(Producer){0};p->mode=mode;xrtAtomic32Store(&p->refs,1,XMEMORY_RELEASE);
    p->promise=xrtPromiseCreate(&p->future,NULL);assert(p->promise&&p->future);
    xrtownershipref ref={p,&ops};
    if(legacy)assert(xrtPromiseProducerBindTakeV1(p->promise,ref,&policy));
    else assert(xrtPromiseProducerBindTakeV2(p->promise,ref,&policy,&cancellation));
}
static void finish(Producer* p)
{
    xrtPromiseDestroy(p->promise);xrtFutureDestroy(p->future);
    assert(!atomicCount(&p->refs)&&atomicCount(&p->drops)==atomicCount(&p->retains)+1&&!xrtGetError());
}
static void invalid(void)
{
    Producer p={0};xrtAtomic32Store(&p.refs,1,XMEMORY_RELEASE);
    p.promise=xrtPromiseCreate(&p.future,NULL);assert(p.promise);
    xrtownershipref ref={&p,&ops};xfutureproducercancellationv1 bad=cancellation;
    assert(!xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,NULL));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    bad.size=0;assert(!xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,&bad));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    bad=cancellation;bad.Retain=NULL;assert(!xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,&bad));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    bad=cancellation;bad.Request=NULL;assert(!xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,&bad));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(atomicCount(&p.refs)==1&&!atomicCount(&p.retains)&&!atomicCount(&p.drops));
    assert(xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,&cancellation));
    assert(!xrtPromiseProducerBindTakeV2(p.promise,ref,&policy,&cancellation));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    finish(&p);
}
static void sequential(void)
{
    Producer p;start(&p,0,false);
    p.refuse=true;assert(!xrtFutureCancel(p.future));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xcancel* token=xrtFutureCancelToken(p.future);assert(token&&!xrtCancelRequested(token));xrtCancelDestroy(token);
    assert(!atomicCount(&p.retains)&&!atomicCount(&p.requests)&&atomicCount(&p.refs)==1);
    p.refuse=false;
    xrtSetErrorKind(XERR_RANGE);const xerror* ambient=xrtGetError();assert(ambient);
    assert(xrtMemDebugFailAfter(0));
    assert(xrtFutureCancel(p.future)&&!xrtMemDebugFailTriggered());
    assert(!xrtFutureCancel(p.future)&&!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();assert(xrtGetError()==ambient);xrtClearError();
    assert(atomicCount(&p.requests)==1&&xrtFutureState(p.future)==XFUTURE_PENDING);
    assert(xrtPromiseResolve(p.promise,&payload)&&!xrtFutureCancel(p.future));finish(&p);
    start(&p,1,false);assert(xrtFutureCancel(p.future));assert(atomicCount(&p.requests)==1);finish(&p);
    start(&p,3,false);assert(xrtFutureCancel(p.future));finish(&p);
    start(&p,0,true);assert(xrtFutureCancel(p.future)&&!atomicCount(&p.requests)&&!atomicCount(&p.retains));finish(&p);
    /* V2 retains the same physical producer-policy admission; no observer or
     * synthetic ownership node is introduced by the cancellation descriptor. */
    start(&p,0,false);xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xfutureproducerownershipv1* policies[]={&policy};
    assert(xrtFutureOwnershipAdapterV2(xrtFutureOwnership(p.future),NULL,0,policies,1));
    assert(xrtOwnershipScopeEnd(&freeze));finish(&p);
}
static int32 cancelWorker(void* data)
{
    Producer* p=data;assert(xrtFutureCancel(p->future)&&!xrtGetError());
    xrtFutureDestroy(p->future);return 0; /* actual independent observer reference */
}
static void race(void)
{
    for(unsigned i=0;i<300;++i){
        Producer p;start(&p,2,false);assert(xrtFutureRef(p.future));
        xthread* thread=xrtThreadCreate(cancelWorker,&p,0);assert(thread);
        while(!atomicCount(&p.entered))xrtThreadYield();
        assert(xrtPromiseResolve(p.promise,&payload));
        assert(atomicCount(&p.refs)==1&&!xrtFutureCancel(p.future));
        xrtAtomic32Store(&p.release,1,XMEMORY_RELEASE);
        assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);
        assert(xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
        assert(atomicCount(&p.requests)==1);finish(&p);
    }
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    invalid();sequential();race();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("Future producer cancellation: exact V2 bind/retain refusal, allocation-free once-only request, ambient error, outside-lock/freeze callback, V1/graph compatibility, 300 terminal-publication races, balanced");
    return 0;
}
