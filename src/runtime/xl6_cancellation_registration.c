#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "xl6_cancellation.h"
#include "xl6_closure.h"
#include "src/api/xl_foreign_error.h"
#include "xl_cancellation_retirement.h"
#include <stdatomic.h>
#include <limits.h>

typedef struct Registration {
    int32_t refs;
    size_t active;
    const void* claim;
    uint64_t id, callbackThread;
    bool publishing, closing, closed, cleared, callbackActive, triggered, failed, pending, retrying;
    xcancelwatch* watch; /* One real registration/Unwatch right, not a list link. */
    XL6_Closure* callback;
    xerror* error;
    unsigned int cleanupKind;
    struct Registration* next;
} Registration;
static _Atomic(uint32_t) gate;
static uint64_t nextId;
static Registration* pending;
static void release(Registration*);
static bool closeRegistration(Registration*);

/* Short metadata transitions only; all invocation, Unwatch waits, capture
 * destruction and code-lease release happen outside gate/world mutation. */
static void enter(xrtownershipscope* mutation)
{
    if (!xrtOwnershipMutationBegin(mutation)) abort();
    uint32_t expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&gate,&expected,1,
        memory_order_acq_rel,memory_order_acquire)) { expected=0; xrtThreadYield(); }
}
static void leave(xrtownershipscope* mutation)
{
    atomic_store_explicit(&gate,0,memory_order_release);
    if (!xrtOwnershipScopeEnd(mutation)) abort();
}
static void retainLocked(Registration* state)
{ if (state->refs <= 0 || state->refs == INT32_MAX) abort(); ++state->refs; }
static void release(Registration* state)
{
    if (!state) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    if (state->refs <= 0) abort();
    if (--state->refs) { leave(&mutation); return; }
    if (state->watch || state->callback || state->active || state->pending || state->publishing) abort();
    xerror* error=state->error; state->error=NULL;
    leave(&mutation); xrtErrorFree(error); xrtFree(state);
}
static Registration* begin(void* raw)
{
    Registration* state=raw;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool ok=state && state->refs>0 && !state->cleared && !state->claim &&
        !state->publishing && !state->pending && state->active<SIZE_MAX;
    if (ok) { retainLocked(state); ++state->active; }
    leave(&mutation);
    if (!ok) xrtSetErrorKind(XERR_STATE);
    return ok ? state : NULL;
}
static void end(Registration* state)
{
    if (!state) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    if (!state->active) abort();
    --state->active;
    leave(&mutation); release(state);
}
static bool closeRegistration(Registration* state)
{
    xrtownershipscope mutation={0}; enter(&mutation);
    if (state->closing) {
        /* A close on another thread can be waiting for this callback. The
         * callback must not wait back on it. Other concurrent closers receive
         * a checked busy/state error instead of spinning or deadlocking. */
        bool self=state->callbackActive && state->callbackThread==xrtThreadCurrentId();
        leave(&mutation);
        if (!self) xrtSetErrorKind(XERR_STATE);
        return self;
    }
    xcancelwatch* watch=state->watch;
    state->closing=true;
    leave(&mutation);
    bool ok=xrtCancelUnwatchChecked(watch);
    enter(&mutation);
    XL6_Closure* callback=NULL;
    if (ok) { state->watch=NULL; state->closed=true; callback=state->callback; state->callback=NULL; }
    state->closing=false;
    leave(&mutation);
    xl6ClosureRuntimeV1Get()->Release(callback);
    return ok;
}
static void retire(Registration* state)
{
    xrtownershipscope mutation={0}; enter(&mutation);
    if (state->pending) abort();
    state->cleanupKind=(unsigned int)(xrtGetError() ? xrtErrorKind(xrtGetError()) : XERR_STATE);
    state->pending=true; state->next=pending; pending=state;
    leave(&mutation);
    /* Transfer the independent creator reference to this intrusive record.
     * No allocation and no disappearing right/lease on failure. */
}
void xl6CancellationRegistrationDestroyV1(void* raw)
{
    Registration* state=raw;
    if (!state) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool cleared=state->cleared;
    bool allowed=!state->claim && !state->publishing;
    leave(&mutation);
    if (cleared) { release(state); return; } /* Plan owns the remaining Finish duty. */
    if (!allowed) xrtSetErrorKind(XERR_STATE);
    bool ok=allowed && closeRegistration(state);
    if (!ok) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); retire(state); return; }
    release(state);
}
static void notify(void* raw)
{
    Registration* state=raw;
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    xrtownershipscope mutation={0}; enter(&mutation);
    if (state->cleared || state->callbackActive) abort();
    retainLocked(state); ++state->active; state->callbackActive=true;
    state->triggered=true;
    state->callbackThread=xrtThreadCurrentId();
    XL6_Closure* callback=runtime->Retain(state->callback);
    leave(&mutation);
    xl6_error_scope work={0};
    if (!xl6ErrorScopeBegin(&work)) abort();
    if (!callback) { xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&work); }
    else {
        XL6_ClosureScope scope={0}; runtime->Enter(&scope,callback);
        bool ok=((bool (*)(void*))callback->invoke)(callback->environment);
        if (!ok) xl6ErrorScopeCaptureFailure(&work);
        enter(&mutation);
        XL6_Closure* stored=state->callback; state->callback=NULL;
        leave(&mutation);
        runtime->Release(stored);
        runtime->Leave(&scope); runtime->Release(callback);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    enter(&mutation);
    if (!outcome.ok) {
        state->failed=true;
        if (!state->error) state->error=xrtErrorRef(outcome.error);
    }
    state->callbackActive=false; state->callbackThread=0;
    leave(&mutation);
    end(state);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    else xrtErrorFree(outcome.error);
}
static void watchDrop(const void* raw) { release((Registration*)raw); }
static bool count(const void* raw,size_t* output)
{
    const Registration* state=raw;
    if (!state || !output || state->refs<=0 || state->publishing || state->active ||
        state->closing || state->cleared) return false;
    *output=(size_t)state->refs; return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Registration* state=raw; size_t refs;
    if (!visit || !count(raw,&refs)) return false;
    return (!state->watch || visit(xrtCancelWatchOwnership(state->watch),context)) &&
        (!state->callback || visit(xl6ClosureRuntimeV1Get()->Ownership(state->callback),context)) &&
        (!state->error || visit(xrtErrorOwnership(state->error),context));
}
static const xrtownershipops ops={count,trace};
static const xcancelwatchownershipv1 policy={sizeof(policy),notify,watchDrop,&ops};
const xcancelwatchownershipv1* xl6CancellationWatchPolicyV1Get(void) { return &policy; }
void* xl6CancellationRegistrationCreateV1(void* token,XL6_Closure* callback)
{
    if (!token || !callback || !callback->invoke) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    Registration* state=xrtCalloc(1,sizeof(*state));
    if (!state) return NULL;
    state->refs=1; state->publishing=true;
    state->callback=xl6ClosureRuntimeV1Get()->Retain(callback);
    if (!state->callback) { xrtFree(state); return NULL; }
    xrtownershipscope mutation={0}; enter(&mutation);
    bool allowed=nextId<UINT64_MAX;
    if (allowed) { state->id=++nextId; retainLocked(state); }
    leave(&mutation);
    if (!allowed) { state->publishing=false; xrtSetErrorKind(XERR_RANGE); goto failed; }
    xcancelwatch* watch=xrtCancelWatchOwnedV1(token,state,&policy);
    enter(&mutation);
    state->watch=watch; state->publishing=false;
    bool failed=state->failed;
    xerror* error=failed ? xrtErrorRef(state->error) : NULL;
    leave(&mutation);
    if (!watch) { release(state); goto failed; } /* OwnedV1 consumed no Data ref. */
    if (failed) { xl6ErrorScopePropagateFailure((xl6_error_result){false,error}); goto failed; }
    return state;
failed: {
        xl6_error_scope cleanup={0};
        if (!xl6ErrorScopeBeginCleanup(&cleanup,true)) abort();
        xl6CancellationRegistrationDestroyV1(state);
        xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
        xrtSetErrorTake(result.error); return NULL;
    }
}
void xl6CancellationRegistrationCloseV1(void* raw)
{
    Registration* state=begin(raw);
    if (!state) return;
    if (!closeRegistration(state) && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    end(state);
}
bool xl6CancellationRegistrationClosedV1(void* raw)
{
    Registration* state=begin(raw);
    if (!state) return false;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool closed=state->closed; leave(&mutation); end(state); return closed;
}
bool xl6CancellationRegistrationFailedV1(void* raw)
{
    Registration* state=begin(raw);
    if (!state) return false;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool failed=state->failed; leave(&mutation); end(state); return failed;
}
bool xl6CancellationRegistrationTriggeredV1(void* raw)
{
    Registration* state=begin(raw);
    if (!state) return false;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool triggered=state->triggered; leave(&mutation); end(state); return triggered;
}
void xl6CancellationRegistrationCheckV1(void* raw)
{
    Registration* state=begin(raw);
    if (!state) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    bool failed=state->failed; xerror* error=failed ? xrtErrorRef(state->error) : NULL;
    leave(&mutation); end(state);
    if (failed) xl6ErrorScopePropagateFailure((xl6_error_result){false,error});
}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    return visit && xrtValueGetHandle(value,&raw,NULL,NULL) && raw &&
        visit((xrtownershipref){raw,&ops},context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6CancellationRegistrationHandlePolicyV1Get,
    xl6CancellationRegistrationDestroyV1,handleTrace)

static bool hold(const void* raw)
{
    Registration* state=(Registration*)raw;
    if (!state || state->refs<=0 || state->refs==INT32_MAX) return false;
    ++state->refs; return true;
}
static void drop(const void* raw) { release((Registration*)raw); }
static bool claim(const void* raw,const void* token)
{
    Registration* state=(Registration*)raw;
    if (!token || state->cleared || state->active || state->publishing || state->closing ||
        (state->claim && state->claim!=token)) return false;
    state->claim=token; return true;
}
static void restore(const void* raw,const void* token)
{
    Registration* state=(Registration*)raw;
    if (state->claim!=token || state->cleared) abort();
    state->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    Registration* state=(Registration*)raw;
    if (state->claim!=token || state->cleared || state->active) abort();
    state->cleared=true; state->closed=true;
}
static xrtownershipfinishresult finish(const void* raw,const void* token)
{
    Registration* state=(Registration*)raw;
    xrtownershipscope mutation={0}; enter(&mutation);
    if (state->claim!=token || !state->cleared) abort();
    retainLocked(state); ++state->active; leave(&mutation);
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    bool ok=closeRegistration(state);
    if (!ok) xl6ErrorScopeCaptureFailure(&work);
    end(state);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return XRT_OWNERSHIP_FINISH_FAILED; }
    xrtErrorFree(outcome.error); return XRT_OWNERSHIP_FINISH_READY;
}
static bool finishBool(const void* raw,const void* token)
{ return finish(raw,token)==XRT_OWNERSHIP_FINISH_READY; }
const xrtownershipadapterv1* xl6CancellationRegistrationAdapterV1(xrtownershipref ref,
    const xrtownershipfinishv1** completion)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finishBool};
    static const xrtownershipfinishv1 final={sizeof(final),&adapter,finish};
    size_t refs;
    if (ref.Ops!=&ops || !completion || !count(ref.Data,&refs)) return NULL;
    *completion=&final; return &adapter;
}
static bool busy(const void* raw)
{
    const Registration* state=raw;
    return state && state->refs>0 && (state->active || state->publishing || state->closing);
}
const xrtownershipquiescencev1* xl6CancellationRegistrationQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 value={sizeof(value),&ops,busy}; return &value; }

static int checkedEnd(xl6_error_scope* scope,bool ok)
{
    if (!ok) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result); else xrtErrorFree(result.error);
    return ok && result.ok;
}
int xlCancellationRetirementSnapshotCheckedV1(XL_CancellationRetirementV1* rows,size_t capacity,size_t* total)
{
    if (total) *total=0;
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    bool ok=total && (!capacity || rows);
    if (!ok) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xrtownershipscope mutation={0}; enter(&mutation);
        size_t count=0;
        for (Registration* state=pending;state;state=state->next) {
            if (count<capacity) rows[count]=(XL_CancellationRetirementV1){state->id,state->cleanupKind};
            if (count==SIZE_MAX) abort();
            ++count;
        }
        *total=count; leave(&mutation);
    }
    return checkedEnd(&work,ok);
}
int xlCancellationRetirementRetryCheckedV1(uint64_t id)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    xrtownershipscope mutation={0}; enter(&mutation);
    Registration* state=pending;
    while (state && state->id!=id) state=state->next;
    bool allowed=state && !state->retrying && !state->claim && !state->publishing;
    if (allowed) { retainLocked(state); ++state->active; state->retrying=true; }
    leave(&mutation);
    bool ok=allowed && closeRegistration(state);
    if (allowed) {
        enter(&mutation); state->retrying=false;
        if (ok) {
            Registration** slot=&pending;
            while (*slot && *slot!=state) slot=&(*slot)->next;
            if (!*slot) abort();
            *slot=state->next; state->next=NULL; state->pending=false;
        }
        leave(&mutation);
        if (ok) release(state); /* Actual registry owner. */
        end(state); /* Independent lookup/activity pin. */
    }
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    return checkedEnd(&work,ok);
}
