#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern void cancel_wait_prepare(void),cancel_wait_finish(void);
extern xvalue* cancel_watch_new(int64_t);
extern xfuture* cancel_wait_launch(xvalue*,xvalue*,xfuture*,xfuture*,int);
static const XL6_ContinuationFrameRuntimeV2* storage;
static const XL6_ContinuationFrameRuntimeV4* cancellation;
static XL6_ContinuationQueue* queue;
static void* frames[4];static unsigned frameCount,drops[3],sequence[2],dropCount,finals,caught,after;
static unsigned executions,races,oomArmed,reentries,refusals,retiredPins,sharedNotifications,sharedReleases;
static int finalRaises,dropRaises;
static xfuture* activeOutput;
static bool cancelBeforeSuccessor;
static unsigned explicitReentries;
void cancel_wait_before_successor(void)
{
    if(cancelBeforeSuccessor){
        cancelBeforeSuccessor=false;
        assert(xrtFutureCancel(activeOutput)&&!xrtGetError());
    }
}
static int64_t payload=10;
XL6_ContinuationQueue* cancel_wait_queue(void) {return queue;}
void cancel_wait_frame(void* frame)
{
    assert(frameCount<4&&!cancellation->CancelWait(frame));++refusals;
    frames[frameCount++]=storage->Retain(frame);assert(frames[frameCount-1]==frame);
}
int cancel_watch_dropped(int id)
{
    assert(id>=1&&id<=2&&!drops[id]&&dropCount<2);++drops[id];sequence[dropCount++]=(unsigned)id;
    assert(activeOutput&&!xrtFutureDone(activeOutput));return dropRaises;
}
int cancel_watch_finalized(void) {assert(!finals);++finals;return finalRaises;}
void cancel_watch_caught(void) {assert(!caught);++caught;}
void cancel_watch_after(void) {assert(!after);++after;}
xfuture* cancel_watch_input(void) {assert(!"reachability-only entry");return NULL;}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(now.LiveCount==before->LiveCount&&now.LiveBytes==before->LiveBytes&&
        now.AllocCount-before->AllocCount==now.FreeCount-before->FreeCount&&
        now.InvalidFreeCount==before->InvalidFreeCount&&now.DoubleFreeCount==before->DoubleFreeCount&&
        now.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void notified(void* data) {(void)data;++sharedNotifications;}
static void released(void* data) {(void)data;++sharedReleases;}
static void reenter(void* data)
{
    /* XRT's legacy cancel observer has its own scope. Same-frame reentry must
     * return before waiting, taking ownership, or dispatching language code. */
    assert(!cancellation->CancelWait(data)&&!caught&&!finals&&!after);++reentries;
}
static void complete(xpromise* promise,unsigned kind)
{
    if(!kind)assert(xrtPromiseResolve(promise,&payload));
    else if(kind==1){xerror* error=xrtErrorCreate(XERR_STATE,"cancel wait",99,"cleanup failed");
        assert(error&&xrtPromiseReject(promise,error));xrtErrorFree(error);}
    else if(kind==2)assert(xrtPromiseCancel(promise));else assert(xrtPromiseClose(promise));
    assert(!xrtGetError());
}
static void reset(int raisingFinally,int raisingDrop)
{
    memset(frames,0,sizeof(frames));memset(drops,0,sizeof(drops));frameCount=dropCount=finals=caught=after=0;
    finalRaises=raisingFinally;dropRaises=raisingDrop;queue=xl6ContinuationQueueCreate(1);assert(queue);
}
static void release_frames(void)
{
    assert(xl6ContinuationQueueIsIdle(queue)&&!xl6ContinuationQueueActivationCount(queue));
    xl6ContinuationQueueClose(queue);
    assert(!xl6ContinuationRunOne(queue)&&xl6ContinuationQueueDestroy(queue));queue=NULL;
    for(unsigned i=0;i<frameCount;++i){
        assert(!cancellation->CancelWait(frames[i]));++refusals;
        xrtownershipref ref=storage->Ownership(frames[i]);xrtownershipscope freeze={0};
        const xrtownershippreparationv1* prep=NULL;size_t count=0;
        assert(xrtOwnershipFreezeTryBegin(&freeze));
        const xrtownershipadapterv1* adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,&prep);
        assert(adapter&&prep&&prep->Ready(ref.Data)&&ref.Ops->Count(ref.Data,&count)&&count==1);
        assert(adapter->Hold(ref.Data)&&ref.Ops->Count(ref.Data,&count)&&count==2);
        assert(xrtOwnershipScopeEnd(&freeze));storage->Release(frames[i]);
        assert(prep->Ready(ref.Data));adapter->Drop(ref.Data);++retiredPins;
    }
}
static void assert_outcome(xfuture* output,unsigned mode,unsigned second,bool wasCancelled)
{
    unsigned expected=wasCancelled?2:0;int code=0;int64_t value=13;
    if(wasCancelled&&mode==1){expected=0;value=37;}
    bool expectedFinally=mode<2||!second;
    if(mode>=2&&second){expected=second;code=second==1?99:0;}
    if(mode==4&&expectedFinally&&!finalRaises){expected=0;value=53;}
    bool internal=false;
    if(expectedFinally&&finalRaises){expected=1;code=finalRaises==1?77:0;internal=finalRaises==2;}
    if(!expected&&dropRaises){expected=1;code=dropRaises==1?92:0;internal=dropRaises==2;}
    xfutureresult result={0};assert(xrtFutureResult(output,&result));
    assert(result.State==(!expected?XFUTURE_RESOLVED:expected==1?XFUTURE_FAILED:expected==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
    if(expected==1){assert(result.Error&&!result.Value);
        assert(internal?xrtErrorKind(result.Error)==XERR_INTERNAL:xrtErrorCode(result.Error)==code);}
    else assert(!result.Error);
    if(!expected)assert(result.Value&&*(int64_t*)result.Value==value);
    assert(finals==(unsigned)expectedFinally&&caught==(unsigned)wasCancelled&&after==(unsigned)!wasCancelled);
    assert(dropCount==2&&sequence[0]==2&&sequence[1]==1&&drops[1]==1&&drops[2]==1);
}
static void interrupt(unsigned mode,unsigned ready,unsigned second,int raisingFinally,int raisingDrop,int ambient)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);reset(raisingFinally,raisingDrop);
    unsigned notifications=sharedNotifications,releases=sharedReleases;
    xfuture *source=NULL,*cleanup=NULL;xpromise* sourcePromise=xrtPromiseCreate(&source,NULL);
    xpromise* cleanupPromise=xrtPromiseCreate(&cleanup,NULL);assert(sourcePromise&&cleanupPromise);
    xfuturewatch shared={0};assert(xrtFutureWatchInit(&shared,notified,released,NULL));
    assert(xrtFutureWatchAdd(source,&shared)==XFUTURE_WATCH_PENDING);
    if(ready)complete(cleanupPromise,second);
    xvalue *a=cancel_watch_new(1),*b=cancel_watch_new(2);assert(a&&b);
    activeOutput=cancel_wait_launch(a,b,source,cleanup,(int)mode);assert(activeOutput&&frameCount==1);
    xrtValueRelease(a);xrtValueRelease(b);
    xcancel* token=xrtFutureCancelToken(activeOutput);assert(token);
    xcancelwatch* watch=xrtCancelWatch(token,reenter,frames[0]);assert(watch);
    xl6_error_scope boundary={0};assert(xl6ErrorScopeBegin(&boundary));
    if(ambient)xl6ErrorScopePropagateFailure((xl6_error_result){false,ambient==1?
        xrtErrorCreate(XERR_STATE,"ambient",31,"owning host failure"):NULL});
    xmemdebugsnapshot transportBefore,transportAfter;xrtMemDebugSnapshot(&transportBefore);
    assert(xrtMemDebugFailAfter(0));
    assert(cancellation->CancelWait(frames[0])&&!xrtMemDebugFailTriggered());++oomArmed;
    xrtMemDebugFailClear();xrtMemDebugSnapshot(&transportAfter);
    assert(transportBefore.AllocCount==transportAfter.AllocCount);
    assert(!cancellation->CancelWait(frames[0]));++refusals;
    xl6_error_result ambientResult=xl6ErrorScopeEnd(&boundary);
    assert(ambientResult.ok==!ambient&&!xrtGetError());
    if(ambient==1)assert(ambientResult.error&&xrtErrorCode(ambientResult.error)==31);
    else assert(!ambientResult.error);
    xrtErrorFree(ambientResult.error);
    assert(xrtCancelRequested(token)&&!xrtFutureDone(source)&&!xrtFutureDone(activeOutput));
    xcancel* sourceToken=xrtFutureCancelToken(source);assert(sourceToken&&!xrtCancelRequested(sourceToken));xrtCancelDestroy(sourceToken);
    assert(sharedNotifications==notifications&&sharedReleases==releases&&!caught&&!after&&!finals);
    assert(xl6ContinuationRunOne(queue)&&caught==1&&!after);
    if(mode>=2){
        assert(frameCount==2);
        /* Cancellation was consumed by the first await, not copied across
         * frames. The finally await must wait for its own real source. */
        assert(!cancellation->CancelWait(frames[1]));++refusals;
        if(!ready){assert(!xl6ContinuationRunOne(queue)&&!xrtFutureDone(activeOutput)&&!finals);complete(cleanupPromise,second);}
        assert(xl6ContinuationRunOne(queue));
    }
    assert_outcome(activeOutput,mode,second,true);
    assert(!xrtFutureDone(source)&&sharedNotifications==notifications&&sharedReleases==releases);
    complete(sourcePromise,0);assert(sharedNotifications==notifications+1&&sharedReleases==releases+1);
    xrtCancelUnwatch(watch);xrtCancelDestroy(token);
    release_frames();xrtFutureDestroy(activeOutput);activeOutput=NULL;
    xrtPromiseDestroy(sourcePromise);xrtPromiseDestroy(cleanupPromise);xrtFutureDestroy(source);xrtFutureDestroy(cleanup);
    assert(!xrtGetError());balance(&before);++executions;
}
typedef struct Race {xatomic32 start,completed;unsigned order;bool accepted;xpromise* source;void* frame;} Race;
static void wait_flag(const xatomic32* flag)
{
    uint64 deadline=xl6MonotonicMillisecondsV1()+UINT64_C(5000000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(xl6MonotonicMillisecondsV1()<deadline);xrtThreadYield();}
}
static int32 cancelling_worker(void* data)
{
    Race* race=data;wait_flag(&race->start);
    if(race->order==1)wait_flag(&race->completed);
    race->accepted=cancellation->CancelWait(race->frame);assert(!xrtGetError());
    if(race->order==2)xrtAtomic32Store(&race->completed,1,XMEMORY_RELEASE);
    return 0;
}
static int32 source_worker(void* data)
{
    Race* race=data;wait_flag(&race->start);
    if(race->order==2)wait_flag(&race->completed);
    complete(race->source,0);
    if(race->order==1)xrtAtomic32Store(&race->completed,1,XMEMORY_RELEASE);
    return 0;
}
static int32 executor_worker(void* data)
{
    Race* race=data;wait_flag(&race->start);
    assert(xl6ContinuationWaitRunOne(queue)&&!xrtGetError());return 0;
}
static void racing(unsigned mode,unsigned order)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);reset(0,0);
    xfuture *source=NULL,*cleanup=NULL;Race race={0};race.order=order;race.source=xrtPromiseCreate(&source,NULL);
    xpromise* cleanupPromise=xrtPromiseCreate(&cleanup,NULL);assert(race.source&&cleanupPromise);
    xvalue *a=cancel_watch_new(1),*b=cancel_watch_new(2);assert(a&&b);
    activeOutput=cancel_wait_launch(a,b,source,cleanup,(int)mode);xrtValueRelease(a);xrtValueRelease(b);
    race.frame=frames[0];
    xthread* workers[3]={xrtThreadCreate(cancelling_worker,&race,0),xrtThreadCreate(source_worker,&race,0),
        xrtThreadCreate(executor_worker,&race,0)};
    assert(workers[0]&&workers[1]&&workers[2]);xrtAtomic32Store(&race.start,1,XMEMORY_RELEASE);
    for(unsigned i=0;i<3;++i){
        assert(xrtThreadWaitFor(workers[i],UINT64_C(10000000))==XWAIT_OK&&!xrtThreadExitCode(workers[i]));
        xrtThreadDestroy(workers[i]);
    }
    if(order==1)assert(!race.accepted);
    if(order==2)assert(race.accepted);
    if(mode>=2){assert(frameCount==2&&!xrtFutureDone(activeOutput));
        if(race.accepted){assert(!cancellation->CancelWait(frames[1]));++refusals;}
        complete(cleanupPromise,0);assert(xl6ContinuationRunOne(queue));}
    assert_outcome(activeOutput,mode,0,race.accepted);release_frames();
    xrtFutureDestroy(activeOutput);activeOutput=NULL;xrtPromiseDestroy(race.source);xrtPromiseDestroy(cleanupPromise);
    xrtFutureDestroy(source);xrtFutureDestroy(cleanup);assert(!xrtGetError());balance(&before);++races;
}
static void explicit_reenter(void* data)
{
    (void)data;assert(!xrtFutureCancel(activeOutput)&&!xrtGetError());++explicitReentries;
}
static void explicit_cancel(unsigned mode,bool duringSuccessor)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);reset(0,0);
    xfuture *source=NULL,*cleanup=NULL;xpromise* first=xrtPromiseCreate(&source,NULL);
    xpromise* last=xrtPromiseCreate(&cleanup,NULL);assert(first&&last);
    xvalue *a=cancel_watch_new(1),*b=cancel_watch_new(2);assert(a&&b);
    activeOutput=cancel_wait_launch(a,b,source,cleanup,(int)mode);assert(activeOutput&&frameCount==1);
    xrtValueRelease(a);xrtValueRelease(b);
    xcancel* token=xrtFutureCancelToken(activeOutput);assert(token);
    xcancelwatch* watch=xrtCancelWatch(token,explicit_reenter,NULL);assert(watch);
    if(duringSuccessor){
        complete(first,0);cancelBeforeSuccessor=true;
        assert(xl6ContinuationRunOne(queue)&&!cancelBeforeSuccessor&&frameCount==2);
        assert(after==1&&!caught&&!finals&&!xrtFutureDone(cleanup));
        /* The explicit request arrived while predecessor RUNNING and before
         * successor Bind/Watch/Commit. Publication must deliver it, not wait
         * for an unrelated source that is still pending. */
        assert(xl6ContinuationRunOne(queue));xfutureresult result={0};
        assert(xrtFutureResult(activeOutput,&result)&&result.State==XFUTURE_CANCELLED);
        assert(after==1&&!caught&&!finals&&dropCount==2);
    }else{
        assert(xrtMemDebugFailAfter(0));
        assert(xrtFutureCancel(activeOutput)&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        assert(!caught&&!after&&!finals&&!xrtFutureDone(activeOutput)&&!xrtFutureDone(source));
        assert(xl6ContinuationRunOne(queue));
        if(mode>=2){
            assert(frameCount==2&&!xrtFutureDone(activeOutput)&&!xl6ContinuationRunOne(queue));
            assert(!xrtFutureCancel(activeOutput)&&!xl6ContinuationRunOne(queue));
            complete(last,0);assert(xl6ContinuationRunOne(queue));
        }
        assert_outcome(activeOutput,mode,0,true);
    }
    xcancel* sourceToken=xrtFutureCancelToken(duringSuccessor?cleanup:source);
    assert(sourceToken&&!xrtCancelRequested(sourceToken));xrtCancelDestroy(sourceToken);
    xrtCancelUnwatch(watch);xrtCancelDestroy(token);release_frames();
    xrtFutureDestroy(activeOutput);activeOutput=NULL;
    xrtPromiseDestroy(first);xrtPromiseDestroy(last);xrtFutureDestroy(source);xrtFutureDestroy(cleanup);
    assert(!xrtGetError());balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));storage=xl6ContinuationFrameRuntimeV2Get();cancellation=xl6ContinuationFrameRuntimeV4Get();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);cancel_wait_prepare();
    assert(!cancellation->CancelWait(NULL)&&xrtGetError());xrtClearError();
    for(unsigned round=0;round<3;++round)for(unsigned mode=0;mode<5;++mode)
    for(unsigned ready=0;ready<2;++ready)for(unsigned second=0;second<4;++second)
    for(int final=0;final<3;++final)for(int drop=0;drop<3;++drop)for(int ambient=0;ambient<3;++ambient){
        if(mode<2&&(ready||second))continue;
        interrupt(mode,ready,second,final,drop,ambient);
    }
    for(unsigned round=0;round<20;++round)for(unsigned mode=0;mode<5;++mode)for(unsigned order=0;order<3;++order)racing(mode,order);
    assert(executions==2106&&races==300&&oomArmed==executions&&reentries==executions);
    assert(sharedNotifications==executions&&sharedReleases==executions&&refusals>10000&&retiredPins>4500);
    printf("Async cancel wait: %u generated interruptions, %u three-thread source/cancel/executor races, %u allocation-free armed-OOM admissions, %u native observer reentries; shared source untouched, once-only cancellation, exact finally/outcome/barrier and real retired-frame pins, balanced memory\n",executions,races,oomArmed,reentries);
    for(unsigned round=0;round<50;++round){
        for(unsigned mode=0;mode<3;++mode)explicit_cancel(mode,false);
        explicit_cancel(2,true);
    }
    assert(explicitReentries==200);cancel_wait_finish();assert(!xrtGetError());balance(&before);
    puts("Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries; no inline CFG, shared source untouched, await-in-finally preserved, balanced memory");
    return 0;
}
