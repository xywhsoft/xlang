#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_SEM
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>

typedef struct Harness {
    uint64_t caller;
    xsem *entered,*proceed;
    atomic_uint bodies,acquires,releases;
    bool gated;
} Harness;
typedef struct Frame {
    xvalue* value;
    unsigned char initialized;
    Harness* host; /* non-owning test coordination, outlives executor join */
    XL6_ContinuationTicket ticket;
    xfuturewatch watch;
    xfuture* source;
    xpromise* outer;
} Frame;
static const XL6_ActivationCaptureSlotV2 captures[]={
    {{{XL6_CAPTURE_XVALUE,offsetof(Frame,value),sizeof(xvalue*)},NULL},offsetof(Frame,initialized)}
};
static const size_t cleanup[]={0};
static const XL6_ActivationCaptureLayoutV2 values={sizeof(values),sizeof(Frame),1,captures,1,cleanup};
static const XL6_ContinuationFrameLayoutV3 layout={sizeof(layout),&values,
    offsetof(Frame,ticket),offsetof(Frame,watch),offsetof(Frame,source),offsetof(Frame,outer)};
static int acquire(void* data)
{ Harness* host=data;atomic_fetch_add(&host->acquires,1);return 1; }
static void release(void* data)
{ Harness* host=data;atomic_fetch_add(&host->releases,1); }
static void run(void* data)
{
    Frame* frame=data;Harness* host=frame->host;
    assert(xrtThreadCurrentId()!=host->caller);
    atomic_fetch_add(&host->bodies,1);
    if(host->gated){assert(xrtSemPost(host->entered));assert(xrtSemWaitFor(host->proceed,10000000)==XWAIT_OK);}
    int64 number=0;assert(frame->value&&xrtValueGetInt(frame->value,&number)&&number==42);
    xl6_error_result result=xl6ContinuationFrameRuntimeV2Get()->Cleanup(frame,(xl6_error_result){true,NULL});
    assert(result.ok&&!result.error&&!frame->value&&!frame->initialized);
    assert(xl6ContinuationComplete(&frame->ticket));
    assert(xrtPromiseResolve(frame->outer,NULL));
}
static xfuture* start(Harness* host,XL6_ContinuationExecutor* executor)
{
    Frame* frame=xl6ContinuationFrameRuntimeV5Get()->Allocate(sizeof(*frame),&layout);
    if(!frame)return NULL;
    frame->value=NULL;frame->initialized=0;frame->host=host;
    frame->value=xrtValueInt(42);
    if(!frame->value){xl6ContinuationFrameRuntimeV2Get()->Discard(frame);return NULL;}
    frame->initialized=1;
    XL6_CodeOwner owner={sizeof(owner),host,acquire,release};
    return xl6ContinuationFrameLaunchV1(frame,executor,run,&owner);
}
static void finish(XL6_ContinuationExecutor* executor)
{
    xl6ContinuationExecutorClose(executor);
    xdeadline limit=xrtDeadlineAfter(10000000);
    while(!xl6ContinuationExecutorDestroy(executor)){assert(!xrtDeadlineExpired(limit));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void queued(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Harness host={0};host.caller=xrtThreadCurrentId();host.gated=true;
    host.entered=xrtSemCreate(0,2);host.proceed=xrtSemCreate(0,2);assert(host.entered&&host.proceed);
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
    xfuture* first=start(&host,executor);assert(first);
    assert(xrtSemWaitFor(host.entered,10000000)==XWAIT_OK);
    xfuture* second=start(&host,executor);assert(second&&!xrtFutureDone(second)&&atomic_load(&host.bodies)==1);
    /* A queued no-await root consumes a real logical credit before its body.
     * Full/Closed reject without running user code or acquiring a leak. */
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!start(&host,executor));xl6_error_result error=xl6ErrorScopeEnd(&scope);
    assert(!error.ok&&error.error&&xrtErrorKind(error.error)==XERR_RANGE);xrtErrorFree(error.error);
    xl6ContinuationExecutorClose(executor);assert(xl6ErrorScopeBegin(&scope));
    assert(!start(&host,executor));error=xl6ErrorScopeEnd(&scope);
    assert(!error.ok&&error.error&&xrtErrorKind(error.error)==XERR_STATE);xrtErrorFree(error.error);
    assert(atomic_load(&host.bodies)==1);
    assert(!xl6ContinuationExecutorDestroyCompleted(executor));
    assert(xrtSemPost(host.proceed)&&xrtSemPost(host.proceed));
    assert(xrtFutureWaitFor(first,10000000)==XWAIT_OK&&xrtFutureWaitFor(second,10000000)==XWAIT_OK);
    assert(xrtSemWaitFor(host.entered,10000000)==XWAIT_OK);
    xrtFutureDestroy(first);xrtFutureDestroy(second);finish(executor);
    assert(atomic_load(&host.bodies)==2&&atomic_load(&host.acquires)==atomic_load(&host.releases));
    xrtSemDestroy(host.entered);xrtSemDestroy(host.proceed);assert(!xrtGetError());balanced(&before);
}
static void allocation_prefixes(void)
{
    unsigned prefixes=0;
    for(unsigned budget=0;budget<64;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Harness host={0};host.caller=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(budget));
        xfuture* output=start(&host,executor);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(outcome.ok==(output!=NULL));
        if(hit){assert(!output&&!outcome.ok&&outcome.error&&xrtErrorKind(outcome.error)==XERR_MEMORY);}
        else{assert(output&&outcome.ok&&!outcome.error);assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK);}
        xrtErrorFree(outcome.error);xrtFutureDestroy(output);finish(executor);
        assert(atomic_load(&host.bodies)==(hit?0u:1u)&&atomic_load(&host.acquires)==atomic_load(&host.releases));
        assert(!xrtGetError());balanced(&before);++prefixes;
        if(!hit){printf("queued root launch: %u complete allocation prefixes, real code leases balanced\n",prefixes);return;}
    }
    assert(!"allocation prefix bound exhausted");
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    for(unsigned i=0;i<32;++i)queued();
    allocation_prefixes();
    puts("queued root launch: 32 real single-worker barriers, Full/Closed no-body refusals, exact memory balance");
    return 0;
}
