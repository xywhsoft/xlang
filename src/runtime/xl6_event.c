#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "xl6_sync_internal.h"
#include "xl6_concurrent_deadline.h"

/* The class's native slot owns the existing authenticated Sync value. Its
 * real native owner/activity pins and checked retirement registry are shared
 * with Mutex/Cond/RWLock, not replaced with a best-effort void destructor.
 * This is a private resident capability identity, not a language class ID. */
#define XL6_EVENT_ID UINT64_C(0x584C364556454E31)
typedef struct XL6_EventOwner { XL6_SyncOwner base; xevent event; } XL6_EventOwner;
static bool unit(XL6_SyncOwner* owner) { return xrtEventUnit(&((XL6_EventOwner*)owner)->event); }

void* xl6EventCreateV1(bool manualReset, bool signaled)
{
    xl6_error_scope work = {0};
    if (!xl6ErrorScopeBegin(&work)) abort();
    XL6_EventOwner* owner = xrtCalloc(1, sizeof(*owner));
    xvalue* value = NULL;
    if (owner) {
        owner->base.refs = 1; owner->base.identity = XL6_EVENT_ID;
        owner->base.kind = XL6_SYNC_EVENT_V1; owner->base.unit = unit;
        atomic_init(&owner->base.gate, 0);
        owner->base.initialized = xrtEventInit(&owner->event, manualReset, signaled);
        if (owner->base.initialized) {
            XL6_SyncOwner* base = &owner->base;
            value = xl6SyncOwnerWrapTakeNative(&base);
            owner = (XL6_EventOwner*)base;
        }
    }
    if (!value) xl6ErrorScopeCaptureFailure(&work);
    if (owner) xl6SyncOwnerDropNative(&owner->base, NULL);
    xl6_error_result result = xl6ErrorScopeEnd(&work);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); xrtValueRelease(value); return NULL; }
    return value;
}

void xl6EventDestroyV1(void* raw) { xrtValueRelease(raw); }
static XL6_EventOwner* begin(void* raw)
{
    XL6_SyncOwner* owner = xl6SyncOwnerBeginNative(raw, XL6_EVENT_ID, XL6_SYNC_EVENT_V1, false);
    if (!owner) xrtSetErrorKind(XERR_STATE);
    return (XL6_EventOwner*)owner;
}
static void finish(xl6_error_scope* work, bool ok)
{
    if (!ok) xl6ErrorScopeCaptureFailure(work);
    xl6_error_result result = xl6ErrorScopeEnd(work);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    else xrtErrorFree(result.error);
}
static void change(void* raw, bool signaled)
{
    xl6_error_scope work = {0};
    if (!xl6ErrorScopeBegin(&work)) abort();
    XL6_EventOwner* owner = begin(raw);
    bool ok = owner && (signaled ? xrtEventSet(&owner->event) : xrtEventReset(&owner->event));
    xl6SyncOwnerEndNative(owner ? &owner->base : NULL);
    finish(&work, ok);
}
void xl6EventSetV1(void* raw) { change(raw, true); }
void xl6EventResetV1(void* raw) { change(raw, false); }
bool xl6EventWaitV1(void* raw, int64_t milliseconds)
{
    xl6_error_scope work = {0};
    if (!xl6ErrorScopeBegin(&work)) abort();
    XL6_DeadlineV1 deadline;
    XL6_EventOwner* owner = NULL;
    xwaitresult waited = XWAIT_ERROR;
    if (xl6ConcurrentDeadline(milliseconds, &deadline)) {
        owner = begin(raw);
        if (owner) waited = xrtEventWaitFor(&owner->event,xl6DeadlineRemainingMillisecondsV1(deadline));
    }
    xl6SyncOwnerEndNative(owner ? &owner->base : NULL);
    finish(&work, waited != XWAIT_ERROR);
    return waited == XWAIT_OK;
}
bool xl6EventTryWaitV1(void* raw) { return xl6EventWaitV1(raw, 0); }

static bool trace(const xvalue* value, xrtownershipvisitor visitor, void* context)
{
    void* raw = NULL;
    return visitor && xrtValueGetHandle(value, &raw, NULL, NULL) && raw &&
        visitor(xrtValueOwnership(raw), context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6EventHandlePolicyV1Get, xl6EventDestroyV1, trace)
