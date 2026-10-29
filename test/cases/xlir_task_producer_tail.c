/* Construct the final producer owner explicitly, then release it on a real
 * native worker. This isolates the gap after Run/Drop/Watch have returned;
 * it does not rely on a scheduler race or on suppressing a STATE diagnostic.
 * Link this unit instead of xl6_task_frame.o; all other runtime is ordinary. */
#define XRT_MODULE_EVENT
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TASK_POOL
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "src/common/xl6_xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_task_frame.c"
#include "src/runtime/xl6_task_pool_internal.h"
#include <assert.h>
#include <stdio.h>

typedef struct Tail {
    XL6_TaskPool* pool;
    xevent* proceed;
    xatomic32 acquired,released,status,clean,freeze;
    TaskFrame* frame;
    bool failAllocation;
} Tail;
static int acquire(void* raw)
{Tail* tail=raw;return xrtAtomic32FetchAdd(&tail->acquired,1,XMEMORY_RELAXED)==0;}
static void release(void* raw)
{
    Tail* tail=raw;
    /* The host callback runs outside RC mutation, so graph admission remains
     * possible. BUSY must come from the real activity, not a held freeze. */
    xrtownershipscope freeze={0};
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    assert(xrtOwnershipScopeEnd(&freeze));xrtAtomic32Store(&tail->freeze,1,XMEMORY_RELEASE);
    if(tail->failAllocation)xrtMemDebugFailAfter(0);
    XL6_ModulePrepareResult result=xl6TaskPoolTryQuiesceV1(tail->pool);
    xrtAtomic32Store(&tail->status,result,XMEMORY_RELEASE);
    xrtAtomic32Store(&tail->clean,xrtGetError()==NULL,XMEMORY_RELEASE);
    if(tail->failAllocation)xrtMemDebugFailClear();
    xrtClearError();xrtAtomic32FetchAdd(&tail->released,1,XMEMORY_RELEASE);
}
static XL6_TaskOutcome unusedRun(xcancel* cancel,void* data,XL6_TaskResult* result)
{(void)cancel;(void)data;(void)result;return XL6_TASK_SUCCESS;}
static void unusedDrop(void* data,void* context){(void)data;(void)context;}
static bool unusedTrace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static xtaskoutcome nativeTail(xcancel* cancel,void* raw,xtaskvalue* result)
{
    Tail* tail=raw;(void)cancel;(void)result;
    assert(xrtEventWaitFor(tail->proceed,UINT64_C(5000000))==XWAIT_OK);
    xl6TaskProducerPolicyV1Get()->Drop(tail->frame);
    return XTASK_SUCCESS;
}
static int transaction(bool fail)
{
    Tail tail={0};tail.failAllocation=fail;
    XL6_TaskPoolConfig config={1,4,0};tail.pool=xl6TaskPoolCreateInternalV1(&config);assert(tail.pool);
    tail.proceed=xrtEventCreate(true,false);assert(tail.proceed);
    XL6_CodeOwner owner={sizeof(owner),&tail,acquire,release};
    void* payload=xl6TaskFrameAllocateInternalV1(&owner,1,unusedRun,unusedDrop,unusedTrace);assert(payload);
    TaskFrame* frame=frame_header(payload);tail.frame=frame;
    static const XL6_TaskFramePoolV1 binding={xl6TaskPoolHoldInternalV1,xl6TaskPoolDropInternalV1,
        xl6TaskPoolOwnership,xl6TaskActivityEnterInternalV1,xl6TaskActivityLeaveInternalV1};
    assert(binding.Hold(tail.pool));
    /* Simulate only the already completed frame slots: the producer owns the
     * sole remaining reference, and all executable payload/source slots are gone. */
    frame->pool=tail.pool;frame->poolBinding=&binding;frame->committed=true;frame->payloadLive=false;
    XL6_TaskNativeScopeV1 submission={0};assert(xl6TaskNativeBeginV1(tail.pool,&submission));
    xfuture* future=xrtTaskSubmit(submission.native,nativeTail,&tail,NULL);assert(future);
    xl6TaskNativeEndV1(&submission);assert(xrtEventSet(tail.proceed));
    assert(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);
    assert(xl6TaskPoolQuiesceInternalV1(tail.pool));
    int ok=xrtAtomic32Load(&tail.acquired,XMEMORY_ACQUIRE)==1 &&
        xrtAtomic32Load(&tail.released,XMEMORY_ACQUIRE)==1 &&
        xrtAtomic32Load(&tail.status,XMEMORY_ACQUIRE)==XL6_MODULE_PREPARE_BUSY &&
        xrtAtomic32Load(&tail.clean,XMEMORY_ACQUIRE)==1 &&
        xrtAtomic32Load(&tail.freeze,XMEMORY_ACQUIRE)==1;
    xrtFutureDestroy(future);assert(xl6TaskPoolDestroyInternalV1(tail.pool));assert(xrtEventDestroy(tail.proceed));
    return ok;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<64;++i)if(!transaction((i&1)!=0)){
        fputs("producer tail attempted to join its own native pool\n",stderr);return 1;
    }
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("producer tail: 64 deterministic BUSY deferrals, 32 allocation failures, outside-mutation host release, balanced");return 0;
}
