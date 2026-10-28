#include "xl6_ws_wait.h"
#include "xl6_future_payload.h"

typedef XL6_WsResource Stream;
typedef struct XL6_WsWaiter {
    struct XL6_WsWaiter* next;
    xpromise* promise;
    xfuture* future;
    xcancelwatch* watch;
    int64_t* result; /* allocated before subscription; notification allocates none */
    uint64_t deadline;
    size_t minimum;
    int kind;
} Waiter;
enum {WRITABLE,DRAINED,CLOSED};
static bool sweep(Stream*,xwsstream*);
static bool schedule(Stream*);

static void nativeDrop(void* data){xl6ExecutionResourceNativeDropV1(data);}
static void posted(xnetworker* worker,void* data)
{
    (void)worker;Stream* r=data;xrtAtomic32Store(&r->sweepQueued,0,XMEMORY_RELEASE);
    XL6_ExecutionScopeV1 scope={0};
    if(!xl6ExecutionResourceEnterV1(&r->base,false,&scope)){xrtClearError();return;}
    xwsstream* s=xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE);
    if(s&&(!sweep(r,s)||!schedule(r))){
        /* Preserve a checked infrastructure failure and wake every observer. */
        xl6WsStreamRecordFailureV2(r,xrtTakeError());
        (void)xrtWsStreamAbort(s);xrtClearError();
    }
    xl6ExecutionResourceLeaveV1(&scope);
}
const xnettaskownershipv1* xl6WsWaitPostPolicyV2Get(void)
{static const xnettaskownershipv1 policy={sizeof(policy),posted,nativeDrop,&xl6ExecutionResourceNativeOpsV1};return &policy;}
static void cancelled(void* data)
{
    Stream* r=data;XL6_ExecutionScopeV1 scope={0};
    if(!xl6ExecutionResourceEnterV1(&r->base,false,&scope)){xrtClearError();return;}
    uint32 expected=0;
    if(xrtAtomic32CompareExchange(&r->sweepQueued,&expected,1,XMEMORY_ACQ_REL,XMEMORY_RELAXED)){
        bool pinned=xl6ExecutionResourceNativePinV1(&r->base);
        bool postedOk=pinned&&xrtNetEnginePostOwnedV1(r->parent->engine,r->worker,r,xl6WsWaitPostPolicyV2Get());
        if(!postedOk){
            if(pinned)xl6ExecutionResourceNativeDropV1(&r->base);
            xrtAtomic32Store(&r->sweepQueued,0,XMEMORY_RELEASE);
            /* Engine shutdown owns final observer cancellation. A live queue
             * failure aborts the physical stream, whose close sweeps waiters. */
            xwsstream* s=xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE);
            if(s)(void)xrtWsStreamAbort(s);
            xrtClearError();
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);
}
const xcancelwatchownershipv1* xl6WsWaitCancelPolicyV2Get(void)
{static const xcancelwatchownershipv1 policy={sizeof(policy),cancelled,xl6ExecutionResourceNativeDropV1,&xl6ExecutionResourceNativeOpsV1};return &policy;}
static void timed(xnetworker* worker,uint64 id,xnetresult result,void* data)
{
    (void)worker;Stream* r=data;XL6_ExecutionScopeV1 scope={0};
    if(!xl6ExecutionResourceEnterV1(&r->base,false,&scope)){xrtClearError();return;}
    if(id==r->waitTimer){
        r->waitTimer=0;r->waitDue=XRT_DEADLINE_NEVER;
        xwsstream* s=xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE);
        if(result==XNET_RESULT_OK&&s){
            if(!sweep(r,s)||!schedule(r)){xl6WsStreamRecordFailureV2(r,xrtTakeError());(void)xrtWsStreamAbort(s);xrtClearError();}
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);
}
const xnettimerownershipv1* xl6WsWaitTimerPolicyV2Get(void)
{static const xnettimerownershipv1 policy={sizeof(policy),timed,nativeDrop,&xl6ExecutionResourceNativeOpsV1};return &policy;}

static void cancelTimer(Stream* r)
{
    uint64_t id=r->waitTimer;r->waitTimer=0;r->waitDue=XRT_DEADLINE_NEVER;
    /* The Engine owns the physical registration and Data pin. An id can have
     * completed concurrently or been cancelled by shutdown; both are terminal.
     * Its callback checks the exact id before touching the current generation. */
    if(id){xerror* prior=xrtTakeError();(void)xrtNetEngineTimerCancel(r->parent->engine,id);xrtClearError();xrtSetErrorTake(prior);}
}
static bool schedule(Stream* r)
{
    uint64_t due=XRT_DEADLINE_NEVER;
    for(Waiter* n=r->waits;n;n=n->next)if(n->deadline<due)due=n->deadline;
    if((!r->waitTimer&&due==XRT_DEADLINE_NEVER)||(r->waitTimer&&r->waitDue==due))return true;
    cancelTimer(r);if(due==XRT_DEADLINE_NEVER)return true;
    if(!xl6ExecutionResourceNativePinV1(&r->base))return false;
    uint64_t id=xrtNetEngineScheduleOwnedV1(r->parent->engine,r->worker,due,r,xl6WsWaitTimerPolicyV2Get());
    if(!id){xl6ExecutionResourceNativeDropV1(&r->base);return false;}
    r->waitTimer=id;r->waitDue=due;return true;
}
static bool releaseWaiter(Waiter* n)
{
    if(n->watch){
        xcancelwatch* watch=n->watch;
        if(!xrtCancelUnwatchChecked(watch))return false;
        n->watch=NULL;
    }
    xrtPromiseDestroy(n->promise);xrtFutureDestroy(n->future);xrtFree(n->result);xrtFree(n);return true;
}
static bool decision(Waiter* n,xwsstream* s,int64_t* result)
{
    if(xrtFutureState(n->future)!=XFUTURE_PENDING){*result=XNET_RESULT_CANCELLED;return true;}
    if(n->kind==CLOSED&&xrtWsStreamState(s)==XWS_STREAM_CLOSED){*result=XNET_RESULT_OK;return true;}
    if(n->kind!=CLOSED&&xrtWsStreamState(s)!=XWS_STREAM_OPEN){*result=XNET_RESULT_CLOSED;return true;}
    if((n->kind==WRITABLE&&xrtWsStreamWritable(s)>=n->minimum)||
       (n->kind==DRAINED&&!xrtWsStreamPending(s))){*result=XNET_RESULT_OK;return true;}
    if(n->watch&&xrtCancelTriggered(n->watch)){*result=XNET_RESULT_CANCELLED;return true;}
    if(n->deadline!=XRT_DEADLINE_NEVER&&xrtDeadlineExpired(n->deadline)){*result=XNET_RESULT_TIMEOUT;return true;}
    return false;
}
static bool sweep(Stream* r,xwsstream* s)
{
    Waiter* ready=NULL;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))return false;
    Waiter** link=&r->waits;
    while(*link){Waiter* n=*link;int64_t result=0;
        if(!decision(n,s,&result)){link=&n->next;continue;}
        *link=n->next;n->next=ready;ready=n;--r->waitCount;*n->result=result;
    }
    if(!xrtOwnershipScopeEnd(&m))abort();
    bool ok=true;
    while(ready){Waiter* n=ready;ready=n->next;n->next=NULL;
        if(xrtFutureState(n->future)==XFUTURE_PENDING){
            bool failed=xrtAtomic32Load(&r->failed,XMEMORY_ACQUIRE)!=0;
            const xerror* error=failed?xrtAtomicPtrLoad(&r->error,XMEMORY_ACQUIRE):xrtWsStreamError(s);
            if(failed||error){if(!xrtPromiseReject(n->promise,error)&&xrtFutureState(n->future)==XFUTURE_PENDING)ok=false;}
            else if(xl6PayloadResolveOwned(n->promise,n->result,XL6_FUTURE_PAYLOAD_VALUE))n->result=NULL;
            else if(xrtFutureState(n->future)==XFUTURE_PENDING)ok=false;
            else xrtClearError(); /* cancellation won after the pending test */
        }
        if(!releaseWaiter(n)){
            if(!xrtOwnershipMutationBegin(&m))abort();
            n->next=r->waits;r->waits=n;++r->waitCount;
            if(!xrtOwnershipScopeEnd(&m))abort();
            ok=false;
        }
    }
    return ok;
}
static bool notifyWaits(Stream* r,xwsstream* s,int kind)
{
    (void)kind;XL6_ExecutionScopeV1 scope={0};
    if(!xl6ExecutionResourceEnterV1(&r->base,false,&scope)){xrtClearError();return true;}
    bool ok=sweep(r,s)&&schedule(r);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
static bool traceWaits(const Stream* r,xrtownershipvisitor visit,void* context)
{
    for(const Waiter* n=r->waits;n;n=n->next){
        if(!visit(xrtPromiseOwnership(n->promise),context)||!visit(xrtFutureOwnership(n->future),context)||
            (n->watch&&!visit(xrtCancelWatchOwnership(n->watch),context)))return false;
    }
    return true;
}
static bool dropWaits(Stream* r)
{
    cancelTimer(r);
    while(r->waits){
        xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))return false;
        Waiter* n=r->waits;r->waits=n->next;n->next=NULL;--r->waitCount;
        if(!xrtOwnershipScopeEnd(&m))abort();
        (void)xrtPromiseCancel(n->promise);
        if(!releaseWaiter(n)){
            if(!xrtOwnershipMutationBegin(&m))abort();
            n->next=r->waits;r->waits=n;++r->waitCount;
            if(!xrtOwnershipScopeEnd(&m))abort();
            return false;
        }
    }
    return true;
}
static const XL6_WsWaitPolicy waitPolicy={notifyWaits,traceWaits,dropWaits};
static xfuture* start(void* raw,int kind,uint64_t minimum,uint64_t deadline,xcancel* cancel)
{
    XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;
    if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return NULL;
    Stream* r=raw;Waiter* n=NULL;xfuture* output=NULL;bool linked=false,ok=false;
    if(!sweep(r,s))goto done;
    if((kind==WRITABLE&&(!minimum||minimum>SIZE_MAX||minimum>xrtWsStreamWriteLimit(s)))||r->waitCount>=1024){
        xrtSetErrorKind(XERR_RANGE);goto done;}
    if(r->waitPolicy&&r->waitPolicy!=&waitPolicy){xrtSetErrorKind(XERR_STATE);goto done;}
    n=xrtCalloc(1,sizeof(*n));if(!n)goto done;
    n->deadline=deadline;n->minimum=(size_t)minimum;n->kind=kind;n->result=xrtMalloc(sizeof(*n->result));
    if(!n->result)goto done;
    n->promise=xrtPromiseCreate(&n->future,NULL);if(!n->promise)goto done;
    output=xrtFutureRef(n->future);if(!output)goto done;
    if(cancel){
        if(!xl6ExecutionResourceNativePinV1(&r->base))goto done;
        n->watch=xrtCancelWatchOwnedV1(cancel,r,xl6WsWaitCancelPolicyV2Get());
        if(!n->watch){xl6ExecutionResourceNativeDropV1(&r->base);goto done;}
    }
    xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))goto done;
    r->waitPolicy=&waitPolicy;n->next=r->waits;r->waits=n;++r->waitCount;linked=true;
    if(!xrtOwnershipScopeEnd(&m))abort();
    ok=sweep(r,s)&&schedule(r);
done:
    if(!ok){
        xerror* error=xrtTakeError();
        if(output)(void)xrtFutureCancel(output);
        xrtFutureDestroy(output);output=NULL;
        if(n&&!linked){
            /* No graph can see this private preparation. Unwatch is always
             * admissible here; a failed checked unlink retains registration. */
            if(!releaseWaiter(n))abort();
        }
        if(linked)(void)sweep(r,s);
        xrtClearError();xrtSetErrorTake(error);
    }
    xl6ExecutionResourceLeaveV1(&scope);if(!ok)xl6WsStreamFailV2();return output;
}
struct xfuture* xl6WsWaitWritableV2(void* r,uint64_t min,uint64_t due){return start(r,WRITABLE,min,due,NULL);}
struct xfuture* xl6WsWaitWritableCancelV2(void* r,uint64_t min,uint64_t due,void* cancel){return start(r,WRITABLE,min,due,cancel);}
struct xfuture* xl6WsWaitDrainedV2(void* r,uint64_t due){return start(r,DRAINED,0,due,NULL);}
struct xfuture* xl6WsWaitDrainedCancelV2(void* r,uint64_t due,void* cancel){return start(r,DRAINED,0,due,cancel);}
struct xfuture* xl6WsWaitClosedV2(void* r,uint64_t due){return start(r,CLOSED,0,due,NULL);}
struct xfuture* xl6WsWaitClosedCancelV2(void* r,uint64_t due,void* cancel){return start(r,CLOSED,0,due,cancel);}
