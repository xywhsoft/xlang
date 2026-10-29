#include "xl6_text_value.h"
#include "xl6_future_value.h"
#include "xl6_native_handle.h"
#include "xl6_promise.h"
#include "src/api/xl_foreign_error.h"
#include <stdatomic.h>
#include <limits.h>
#include <stdlib.h>

typedef struct Promise {
    int32_t refs;
    size_t active;
    const void* claim;
    bool owner,cleared,closing,settling,completed;
    XL6_FutureValueProfileV1 profile;
    xpromise* producer;
    xfuture* consumer;
} Promise;
static _Atomic(uint32_t) gate;
static void release(Promise*);
static void enter(xrtownershipscope* mutation)
{
    if(!xrtOwnershipMutationBegin(mutation))abort();
    uint32_t expected=0;
    while(!atomic_compare_exchange_weak_explicit(&gate,&expected,1,
        memory_order_acq_rel,memory_order_acquire)) {expected=0;xrtThreadYield();}
}
static void leave(xrtownershipscope* mutation)
{
    atomic_store_explicit(&gate,0,memory_order_release);
    if(!xrtOwnershipScopeEnd(mutation))abort();
}
static void retainLocked(Promise* state)
{if(state->refs<=0 || state->refs==INT32_MAX)abort();++state->refs;}
/* Creator duty and physical pins are distinct. Last language-owner release
 * closes now even when a ROOTED inspection plan still holds the frame. During
 * a claim, the actual plan pins preserve the duty: abort restores then Drop
 * closes; Clear/Finish can detach quarantined endpoints before creator Drop.
 * No hidden registry owner, cancellation surrogate or ignored producer edge. */
static bool cleanup(Promise* state,bool forced)
{
    xrtownershipscope mutation={0};enter(&mutation);
    bool done=!state->producer && !state->consumer;
    bool allowed=!done && (!state->owner || forced) && !state->active && !state->closing &&
        !state->settling && (!state->claim || state->cleared);
    if(!allowed){leave(&mutation);return done;}
    retainLocked(state);state->closing=true;
    xpromise* producer=state->producer;xfuture* consumer=state->consumer;
    state->producer=NULL;state->consumer=NULL;
    leave(&mutation);
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    xrtPromiseDestroy(producer);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xrtFutureDestroy(consumer);
    if(xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    enter(&mutation);state->closing=false;leave(&mutation);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    release(state);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return true;
}
static void release(Promise* state)
{
    if(!state)return;
    (void)cleanup(state,false);
    xrtownershipscope mutation={0};enter(&mutation);
    if(state->refs<=0)abort();
    bool last=--state->refs==0;
    if(last && (state->owner || state->active || state->closing || state->producer || state->consumer ||
        (state->claim && !state->cleared)))abort();
    leave(&mutation);
    if(last)xrtFree(state);
}
static Promise* begin(void* raw)
{
    Promise* state=raw;xrtownershipscope mutation={0};enter(&mutation);
    bool ok=state && state->refs>0 && state->owner && !state->cleared && !state->claim &&
        !state->closing && state->producer && state->consumer;
    if(ok){retainLocked(state);++state->active;}
    leave(&mutation);
    if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorBuild(
        &(xerrordesc){XERR_STATE,0,0,"xlang","Promise","Promise is not available","StateError",NULL})});
    return ok?state:NULL;
}
static void end(Promise* state)
{
    xrtownershipscope mutation={0};enter(&mutation);
    if(!state->active)abort();
    --state->active;leave(&mutation);release(state);
}
static bool matches(Promise* state,XL6_FutureValueProfileV1 profile)
{
    XL6_FutureValueProfileV1 stored=state->profile;
    if(profile.size==stored.size && profile.kind==stored.kind && profile.valueSize==stored.valueSize &&
        profile.type==stored.type && profile.native==stored.native)return true;
    xrtSetErrorKind(XERR_TYPE);
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;
}
static void* create(XL6_FutureValueProfileV1 profile,xcancel* parent)
{
    if(!xl6FutureValueProfileValidV1(profile)) {
        xrtSetErrorKind(XERR_ARGUMENT);goto failed;
    }
    Promise* state=xrtCalloc(1,sizeof(*state));
    if(!state)goto failed;
    state->refs=1;state->owner=true;state->profile=profile;
    state->producer=xrtPromiseCreate(&state->consumer,parent);
    if(state->producer)return state;
    {
        xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,true))abort();
        state->owner=false;release(state);
        xl6_error_result result=xl6ErrorScopeEnd(&scope);xrtSetErrorTake(result.error);
    }
failed:
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
}
void* xl6PromiseCreateV1(XL6_FutureValueProfileV1 profile){return create(profile,NULL);}
void* xl6PromiseCreateParentV1(XL6_FutureValueProfileV1 profile,void* parent)
{
    if(parent)return create(profile,parent);
    xrtSetErrorKind(XERR_ARGUMENT);
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
}
void xl6PromiseDestroyV1(void* raw)
{
    Promise* state=raw;if(!state)return;
    xrtownershipscope mutation={0};enter(&mutation);
    if(!state->owner)abort();
    state->owner=false;leave(&mutation);release(state);
}
xfuture* xl6PromiseFutureV1(XL6_FutureValueProfileV1 profile,void* raw)
{
    Promise* state=begin(raw);if(!state)return NULL;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    xfuture* result=matches(state,profile)?xrtFutureRef(state->consumer):NULL;
    if(!result)xl6ErrorScopeCaptureFailure(&scope);
    end(state);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
/* A separate admission bit arbitrates ONLY the owning producer's completion
 * attempts. It does not replace XRT publication. No gate spans allocation,
 * native completion, cancellation notification, payload cleanup or callbacks.
 * false means another completion has been admitted or the Future is terminal;
 * failed preparation consumes no admission and permits a later attempt. */
static bool available(Promise* state)
{
    bool nativeDone=xrtPromiseDone(state->producer);
    xrtownershipscope mutation={0};enter(&mutation);
    if(nativeDone)state->completed=true;
    bool ok=!state->completed && !state->settling;leave(&mutation);return ok;
}
static bool reserve(Promise* state)
{
    xrtownershipscope mutation={0};enter(&mutation);
    bool ok=!state->completed && !state->settling;
    if(ok)state->settling=true;
    leave(&mutation);return ok;
}
static void complete(Promise* state,bool published)
{
    bool done=published || xrtPromiseDone(state->producer);
    xrtownershipscope mutation={0};enter(&mutation);
    if(!state->settling)abort();
    state->settling=false;state->completed=done;leave(&mutation);
}
bool xl6PromiseResolveV1(XL6_FutureValueProfileV1 profile,void* raw,const void* source)
{
    Promise* state=begin(raw);if(!state)return false;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    void* copy=NULL;bool published=false;
    if(matches(state,profile) && available(state)) {
        if(!xl6FutureValueCopyV1(profile,source,&copy))xl6ErrorScopeCaptureFailure(&scope);
        else if(reserve(state)) {
            published=xl6FutureValuePublishV1(profile,state->producer,copy);
            if(published)copy=NULL;else xl6ErrorScopeCaptureFailure(&scope);
            complete(state,published);
        }
    }
    xl6FutureValueDropV1(profile,copy);
    end(state);xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
    return published;
}
static bool terminal(void* raw,unsigned mode,const xerror* error)
{
    Promise* state=begin(raw);if(!state)return false;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    bool published=false;
    if(!mode && !error){xrtSetErrorKind(XERR_ARGUMENT);xl6ErrorScopeCaptureFailure(&scope);}
    else if(available(state) && reserve(state)) {
        published=mode==0?xrtPromiseReject(state->producer,error):mode==1?
            xrtPromiseCancel(state->producer):xrtPromiseClose(state->producer);
        if(!published)xl6ErrorScopeCaptureFailure(&scope);
        complete(state,published);
    }
    end(state);xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
    return published;
}
bool xl6PromiseRejectV1(void* state,const xerror* error){return terminal(state,0,error);}
bool xl6PromiseCancelV1(void* state){return terminal(state,1,NULL);}
bool xl6PromiseCloseV1(void* state){return terminal(state,2,NULL);}
bool xl6PromiseDoneV1(void* raw)
{Promise* state=begin(raw);if(!state)return false;bool result=xrtPromiseDone(state->producer);end(state);return result;}
bool xl6PromiseRequestedV1(void* raw)
{
    Promise* state=begin(raw);if(!state)return false;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    xcancel* token=xrtPromiseCancelToken(state->producer);
    bool result=token && xrtCancelRequested(token);
    if(!token)xl6ErrorScopeCaptureFailure(&scope);
    xrtCancelDestroy(token);end(state);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
static bool count(const void* raw,size_t* output)
{
    const Promise* state=raw;
    if(!state || !output || state->refs<=0 || state->active || state->closing || state->settling || state->cleared)return false;
    *output=(size_t)state->refs;return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Promise* state=raw;size_t refs;
    /* These two edges can have identical physical identities. They are TWO
     * real endpoint references, not a deduplicated logical relationship. */
    return visit && count(raw,&refs) &&
        (!state->producer || visit(xrtPromiseOwnership(state->producer),context)) &&
        (!state->consumer || visit(xrtFutureOwnership(state->consumer),context));
}
static const xrtownershipops ops={count,trace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    return visit && xrtValueGetHandle(value,&raw,NULL,NULL) && raw && visit((xrtownershipref){raw,&ops},context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6PromiseHandlePolicyV1Get,xl6PromiseDestroyV1,handleTrace)
static bool hold(const void* raw)
{Promise* state=(Promise*)raw;if(!state || state->refs<=0 || state->refs==INT32_MAX)return false;++state->refs;return true;}
static void drop(const void* raw){release((Promise*)raw);}
static bool claim(const void* raw,const void* token)
{
    Promise* state=(Promise*)raw;
    if(!token || state->cleared || state->active || state->closing || state->settling ||
       (state->claim && state->claim!=token))return false;
    state->claim=token;return true;
}
static void restore(const void* raw,const void* token)
{Promise* state=(Promise*)raw;if(state->claim!=token || state->cleared)abort();state->claim=NULL;}
static void clear(const void* raw,const void* token)
{Promise* state=(Promise*)raw;if(state->claim!=token || state->cleared || state->active || state->closing)abort();state->cleared=true;}
static xrtownershipfinishresult finish(const void* raw,const void* token)
{
    Promise* state=(Promise*)raw;
    if(state->claim!=token || !state->cleared)abort();
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    bool done=cleanup(state,true);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return XRT_OWNERSHIP_FINISH_FAILED;}
    xrtErrorFree(result.error);return done?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_BUSY;
}
static bool finishBool(const void* raw,const void* token){return finish(raw,token)==XRT_OWNERSHIP_FINISH_READY;}
const xrtownershipadapterv1* xl6PromiseOwnershipAdapterV1(xrtownershipref ref,const xrtownershipfinishv1** completion)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finishBool};
    static const xrtownershipfinishv1 final={sizeof(final),&adapter,finish};size_t refs;
    if(ref.Ops!=&ops || !completion || !count(ref.Data,&refs))return NULL;
    *completion=&final;return &adapter;
}
static bool busy(const void* raw)
{const Promise* state=raw;return state && state->refs>0 && (state->active || state->closing || state->settling);}
const xrtownershipquiescencev1* xl6PromiseQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 value={sizeof(value),&ops,busy};return &value;}
