#define XRT_MODULE_SEM
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_semaphore.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct SemOwner {
    volatile int32_t refs;
    uint64_t identity;
    size_t active;
    const void* claim;
    bool initialized, cleared;
    xsem storage;
} SemOwner;

static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static const xvaluehandleops handleOps;

static SemOwner* require(const xvalue* value, uint64_t identity)
{
    void* raw = NULL;
    void* context = NULL;
    const xvaluehandleops* actual = NULL;
    if (!value || !identity || xrtValueType(value) != XVALUE_HANDLE ||
        xrtValueTypeId(value) != identity ||
        !xrtValueGetHandle(value, &raw, &actual, &context) ||
        actual != &handleOps || context || !raw) return NULL;
    SemOwner* owner = raw;
    return owner->identity == identity && !owner->cleared && owner->initialized ? owner : NULL;
}

bool xl6SemAdmitsV1(const xvalue* value, uint64_t identity)
{
    xrtownershipscope scope = {0};
    mutationBegin(&scope);
    bool ok = require(value, identity) != NULL;
    mutationEnd(&scope);
    return ok;
}

static void drop(void* raw, void* context)
{
    (void)context;
    SemOwner* owner = raw;
    if (!owner) return;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    int32_t refs = xrtRefRelease(&owner->refs);
    if (refs < 0) abort();
    if (refs) { mutationEnd(&mutation); return; }
    if (owner->active) abort();
    mutationEnd(&mutation);
    /* Private semaphores have no held-lock state. Every waiter/native method
     * owns an activity pin until AFTER its XRT call returns. A quiescent,
     * initialized private semaphore cannot legally refuse Unit; refusal is a
     * broken native backend invariant, never a language timeout/status result.
     * Retryable graph Finish below retains its real plan pin on failure. */
    if (owner->initialized && !xrtSemUnit(&owner->storage)) abort();
    xrtFree(owner);
}

static bool clone(void* raw, void** output, void* context)
{
    (void)context;
    SemOwner* owner = raw;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    bool ok = owner && output && !owner->cleared && owner->initialized &&
        xrtRefRetain(&owner->refs) > 0;
    if (ok) *output = owner;
    else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation);
    return ok;
}
static const xvaluehandleops handleOps = {clone, drop, NULL, NULL};

static bool count(const void* raw, size_t* result)
{
    const SemOwner* owner = raw;
    if (!owner || !result || owner->refs <= 0 || owner->active || owner->cleared ||
        !owner->initialized) return false;
    *result = (size_t)owner->refs;
    return true;
}
static bool trace(const void* raw, xrtownershipvisitor visit, void* context)
{
    (void)context;
    size_t refs;
    return visit && count(raw, &refs);
}
static const xrtownershipops ownerOps = {count, trace};
static bool handleTrace(const xvalue* value, xrtownershipvisitor visit, void* context)
{
    void* raw = NULL;
    void* user = NULL;
    const xvaluehandleops* actual = NULL;
    return visit && xrtValueGetHandle(value, &raw, &actual, &user) &&
        actual == &handleOps && raw && !user && visit((xrtownershipref){raw, &ownerOps}, context);
}
static bool hold(const void* raw)
{
    SemOwner* owner = (SemOwner*)raw;
    return owner && owner->refs > 0 && owner->refs < INT32_MAX && xrtRefRetain(&owner->refs) > 0;
}
static void release(const void* raw) { drop((void*)raw, NULL); }
static bool claim(const void* raw, const void* token)
{
    SemOwner* owner = (SemOwner*)raw;
    if (!token || owner->active || owner->cleared || (owner->claim && owner->claim != token)) return false;
    owner->claim = token;
    return true;
}
static void restore(const void* raw, const void* token)
{
    SemOwner* owner = (SemOwner*)raw;
    if (!token || owner->claim != token || owner->cleared) abort();
    owner->claim = NULL;
}
static void clear(const void* raw, const void* token)
{
    SemOwner* owner = (SemOwner*)raw;
    if (!token || owner->claim != token || owner->active || owner->cleared) abort();
    owner->cleared = true;
}
static bool finish(const void* raw, const void* token)
{
    SemOwner* owner = (SemOwner*)raw;
    if (!token || owner->claim != token || !owner->cleared || owner->active) abort();
    if (!owner->initialized) return true;
    /* Own checked completion, independent of the caller's prior failure. */
    xl6_error_scope work = {0};
    if (!xl6ErrorScopeBegin(&work)) abort();
    bool ok = xrtSemUnit(&owner->storage);
    if (!ok) xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result result = xl6ErrorScopeEnd(&work);
    if (ok) owner->initialized = false;
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return ok;
}
const xrtownershipadapterv1* xl6SemOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), hold, release,
        claim, restore, NULL, clear, finish};
    size_t refs;
    if (ref.Data && ref.Ops == &ownerOps) return count(ref.Data, &refs) ? &adapter : NULL;
    return xrtValueHandleOwnershipAdapterV1(ref, &handleOps, handleTrace);
}

xvalue* xl6SemCreateV1(uint64_t identity, int64_t initial, int64_t maximum)
{
    xl6_error_scope scope = {0};
    if (!xl6ErrorScopeBegin(&scope)) abort();
    xvalue* value = NULL;
    SemOwner* owner = NULL;
    if (!identity || initial < 0 || maximum <= 0 || initial > maximum ||
        (uint64_t)maximum > UINT32_MAX) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        owner = xrtCalloc(1, sizeof(*owner));
        if (!owner) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); }
        else {
            owner->refs = 1; owner->identity = identity;
            owner->initialized = xrtSemInit(&owner->storage, (uint32_t)initial, (uint32_t)maximum);
            if (owner->initialized) value = xrtValueHandleTake((void**)&owner, &handleOps, NULL);
            if (value && (!xrtValueTypeIdBind(value, identity) ||
                !xrtValueHandleOwnershipBindPhased(value, handleTrace))) {
                xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(value); value = NULL;
            }
        }
    }
    if (!value) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) drop(owner, NULL);
    xl6_error_result result = xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); xrtValueRelease(value); return NULL; }
    return value;
}
xvalue* xl6SemCreateDefaultV1(uint64_t identity)
{ return xl6SemCreateV1(identity, 0, INT32_MAX); }

static SemOwner* begin(const xvalue* value, uint64_t identity)
{
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    SemOwner* owner = require(value, identity);
    if (!owner || owner->active == SIZE_MAX || xrtRefRetain(&owner->refs) <= 0) owner = NULL;
    else ++owner->active;
    mutationEnd(&mutation);
    return owner;
}
static bool run(const xvalue* value, uint64_t identity, unsigned operation, int64_t argument)
{
    /* An ordinary isolated scope contains transient native errors even when
     * the caller has an escaping error. Ending it restores that original;
     * bool status is the contract, so never publish the transient failure. */
    xl6_error_scope scope = {0};
    if (!xl6ErrorScopeBegin(&scope)) abort();
    SemOwner* owner = begin(value, identity);
    bool ok = false;
    if (owner) {
        switch (operation) {
        case 0: ok = xrtSemWait(&owner->storage) == 0; break;
        case 1: ok = xrtSemTryWait(&owner->storage) == 0; break;
        case 2: {
            uint64_t timeout = argument <= 0 ? 0 :
                (uint64_t)argument > UINT64_MAX / 1000u ? UINT64_MAX : (uint64_t)argument * 1000u;
            ok = xrtSemWaitFor(&owner->storage, timeout) == 0;
            break;
        }
        case 3: ok = xrtSemPost(&owner->storage); break;
        case 4:
            if (argument > 0 && (uint64_t)argument <= UINT32_MAX)
                ok = xrtSemPostMany(&owner->storage, (uint32_t)argument);
            break;
        default: abort();
        }
        xrtownershipscope mutation = {0}; mutationBegin(&mutation);
        if (!owner->active) abort();
        --owner->active;
        mutationEnd(&mutation);
        drop(owner, NULL);
    }
    xl6_error_result result = xl6ErrorScopeEnd(&scope);
    xrtErrorFree(result.error);
    return ok;
}
bool xl6SemWaitV1(const xvalue* value, uint64_t identity) { return run(value, identity, 0, 0); }
bool xl6SemTryWaitV1(const xvalue* value, uint64_t identity) { return run(value, identity, 1, 0); }
bool xl6SemWaitTimeoutV1(const xvalue* value, uint64_t identity, int64_t timeout) { return run(value, identity, 2, timeout); }
bool xl6SemPostV1(const xvalue* value, uint64_t identity) { return run(value, identity, 3, 0); }
bool xl6SemPostMultipleV1(const xvalue* value, uint64_t identity, int64_t count) { return run(value, identity, 4, count); }
