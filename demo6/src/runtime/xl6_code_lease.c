#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "xl6_code_lease.h"
#include "xl6_ownership_scope.h"
#include <stdlib.h>
struct XL6_CodeLease {
    xatomic64 references;
    void* context;
    int (*acquire)(void*);
    void (*release)(void*);
    const xrtownershipops* ownerOps;
    const void* claim;
    bool cleared;
};
struct XL6_CodeDrop {
    XL6_CodeLease* lease;
    XL6_CodeDropProc drop;
    void* context;
};
static XL6_CodeLeaseAdmission xl6OwnershipBody_xl6CodeLeaseAcquire(const XL6_CodeOwner* owner, XL6_CodeLease** output)
{
    XL6_CodeLease* lease;
    XL6_CodeLeaseAdmission admission = XL6_CODE_LEASE_CLOSED;
    xerror* prior;
    if (output == NULL) return XL6_CODE_LEASE_INVALID;
    *output = NULL;
    if (owner == NULL) return XL6_CODE_LEASE_ACQUIRED;
    if (owner->size < sizeof(*owner) || owner->acquire == NULL || owner->release == NULL ||
        (xl6CodeOwnerGraphOps(owner) != NULL && owner->context == NULL))
        return XL6_CODE_LEASE_INVALID;
    prior = xrtTakeError();
    if (!owner->acquire(owner->context)) goto done;
    lease = (XL6_CodeLease*)xrtMalloc(sizeof(*lease));
    if (lease == NULL) {
        xerror* failure = xrtTakeError();
        owner->release(owner->context);
        xrtClearError(); xrtSetErrorTake(failure);
        admission = XL6_CODE_LEASE_RESOURCE_FAILURE; goto done;
    }
    xrtAtomic64Init(&lease->references, 1);
    lease->context = owner->context; lease->acquire = owner->acquire; lease->release = owner->release;
    lease->ownerOps = xl6CodeOwnerGraphOps(owner);
    lease->claim = NULL; lease->cleared = false;
    *output = lease; admission = XL6_CODE_LEASE_ACQUIRED;
done:
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return admission;
}
XL6_CodeLeaseAdmission xl6CodeLeaseAcquire(const XL6_CodeOwner* owner, XL6_CodeLease** output)
{
    XL6_OWNERSHIP_RETURN(XL6_CodeLeaseAdmission, xl6OwnershipBody_xl6CodeLeaseAcquire(owner, output));
}
static bool code_lease_ownership_count(const void* data, size_t* count)
{
    const XL6_CodeLease* lease = (const XL6_CodeLease*)data;
    uint64 references;
    if (lease == NULL || count == NULL || lease->cleared || lease->ownerOps == NULL || lease->context == NULL)
        return false;
    references = xrtAtomic64Load(&lease->references, XMEMORY_ACQUIRE);
    if (references == 0 || references > SIZE_MAX) return false;
    *count = (size_t)references; return true;
}
static bool code_lease_ownership_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_CodeLease* lease = (const XL6_CodeLease*)data;
    size_t count;
    return visit != NULL && code_lease_ownership_count(data, &count) &&
        visit((xrtownershipref){lease->context, lease->ownerOps}, context);
}
static const xrtownershipops code_lease_ownership_ops = {
    code_lease_ownership_count, code_lease_ownership_trace
};
xrtownershipref xl6CodeLeaseOwnership(const XL6_CodeLease* lease)
{ return (xrtownershipref){lease, lease != NULL ? &code_lease_ownership_ops : NULL}; }
bool xl6CodeLeaseOwnerMatches(const XL6_CodeLease* lease,const XL6_CodeOwner* owner)
{
    return lease && owner && owner->size>=sizeof(*owner) && owner->context &&
        owner->acquire && owner->release && xl6CodeOwnerGraphOps(owner) &&
        !lease->cleared && lease->context==owner->context &&
        lease->acquire==owner->acquire && lease->release==owner->release &&
        lease->ownerOps==xl6CodeOwnerGraphOps(owner);
}
static bool code_lease_hold(const void* data)
{
    XL6_CodeLease* lease = (XL6_CodeLease*)data;
    uint64 refs = xrtAtomic64Load(&lease->references, XMEMORY_RELAXED);
    if (!refs || refs == UINT64_MAX || refs == SIZE_MAX) return false;
    xl6CodeLeaseRetain(lease); return true;
}
static void code_lease_drop(const void* data) { xl6CodeLeaseRelease((XL6_CodeLease*)data); }
static bool code_lease_claim(const void* data, const void* token)
{
    XL6_CodeLease* lease = (XL6_CodeLease*)data;
    if (lease->cleared || (lease->claim && lease->claim != token)) return false;
    lease->claim = token; return true;
}
static void code_lease_restore(const void* data, const void* token)
{
    XL6_CodeLease* lease = (XL6_CodeLease*)data;
    if (lease->claim != token || lease->cleared) abort();
    lease->claim = NULL;
}
static void code_lease_clear(const void* data, const void* token)
{
    XL6_CodeLease* lease = (XL6_CodeLease*)data;
    if (lease->claim != token || lease->cleared) abort();
    lease->cleared = true;
    /* Retired code dependency remains physically owned until the last pin or
     * descriptor-owning parent is dropped. No host callback under Freeze. */
}
const xrtownershipadapterv1* xl6CodeLeaseOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), code_lease_hold, code_lease_drop,
        code_lease_claim, code_lease_restore, NULL, code_lease_clear, NULL};
    const XL6_CodeLease* lease;
    if (!ref.Data || ref.Ops != &code_lease_ownership_ops) return NULL;
    lease = ref.Data;
    return !lease->cleared && lease->ownerOps && lease->context ? &adapter : NULL;
}
static XL6_CodeLease* xl6OwnershipBody_xl6CodeLeaseRetain(XL6_CodeLease* lease)
{
    uint64 references;
    if (lease == NULL) return NULL;
    references = xrtAtomic64Load(&lease->references, XMEMORY_RELAXED);
    do {
        if (references == 0 || references == UINT64_MAX) abort();
    } while (!xrtAtomic64CompareExchange(&lease->references, &references,
        references + 1, XMEMORY_RELAXED, XMEMORY_RELAXED));
    return lease;
}
XL6_CodeLease* xl6CodeLeaseRetain(XL6_CodeLease* lease)
{
    XL6_OWNERSHIP_RETURN(XL6_CodeLease*, xl6OwnershipBody_xl6CodeLeaseRetain(lease));
}
static void xl6OwnershipBody_xl6CodeLeaseRelease(XL6_CodeLease* lease,
    void** context, void (**release)(void*))
{
    uint64 references;
    if (lease == NULL) return;
    references = xrtAtomic64FetchSub(&lease->references, 1, XMEMORY_ACQ_REL);
    if (references == 0) abort();
    if (references != 1) return;
    *context = lease->context; *release = lease->release;
    xrtFree(lease);
}
void xl6CodeLeaseRelease(XL6_CodeLease* lease)
{
    xrtownershipscope mutation = {0};
    void* context = NULL; void (*release)(void*) = NULL;
    if (lease == NULL) return;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    xl6OwnershipBody_xl6CodeLeaseRelease(lease, &context, &release);
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    /* The detached local still owns the actual host acquisition until this
     * callback returns. Host retirement may inspect, run cleanup or join work;
     * it must not inherit this RC transition's mutation admission. A caller
     * with its own enclosing mutation remains responsible for that boundary. */
    if (release != NULL) {
        xerror* prior = xrtTakeError(); release(context);
        xrtClearError(); xrtSetErrorTake(prior);
    }
}

/* Stack records, not a TLS map allocated during an OOM destructor. The lock
 * only protects links and borrowed identity lookup; no host/generated callback
 * or last reference release runs while it is held. Concurrent threads have
 * distinct logical stacks even though the intrusive list is shared. */
static xatomic64 code_scope_lock = {0};
static XL6_CodeScope* code_scopes;
bool xl6CodeOwnerScopeActive(const XL6_CodeOwner* owner)
{
    const XL6_CodeScope* scope;
    if (!owner) return false;
    for (scope = code_scopes; scope; scope = scope->next) {
        const XL6_CodeLease* lease = scope->lease;
        if (lease && lease->context == owner->context && lease->acquire == owner->acquire &&
            lease->release == owner->release) return true;
    }
    return false;
}
bool xl6CodeOwnerScopeForeign(const XL6_CodeOwner* owner)
{
    const XL6_CodeScope* scope;
    uint64 thread = xrtThreadCurrentId();
    if (!owner) return false;
    for (scope = code_scopes; scope; scope = scope->next) {
        const XL6_CodeLease* lease = scope->lease;
        if (scope->threadId != thread && lease && lease->context == owner->context &&
            lease->acquire == owner->acquire && lease->release == owner->release) return true;
    }
    return false;
}
static void code_scope_lock_enter(void)
{
    uint64 expected;
    do {
        expected = 0;
        if (xrtAtomic64CompareExchange(&code_scope_lock, &expected, 1,
                XMEMORY_ACQUIRE, XMEMORY_RELAXED)) return;
        xrtThreadYield();
    } while (true);
}
static void code_scope_lock_leave(void)
{ xrtAtomic64Store(&code_scope_lock, 0, XMEMORY_RELEASE); }

static void xl6OwnershipBody_xl6CodeScopeEnter(XL6_CodeScope* scope, XL6_CodeLease* lease)
{
    if (scope == NULL || scope->lease != NULL || scope->previous != NULL || scope->next != NULL) abort();
    if (lease == NULL) return;
    scope->lease = xl6CodeLeaseRetain(lease);
    scope->threadId = xrtThreadCurrentId();
    code_scope_lock_enter();
    scope->next = code_scopes;
    if (code_scopes != NULL) code_scopes->previous = scope;
    code_scopes = scope;
    code_scope_lock_leave();
}
void xl6CodeScopeEnter(XL6_CodeScope* scope, XL6_CodeLease* lease)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_xl6CodeScopeEnter(scope, lease));
}
static XL6_CodeLease* xl6OwnershipBody_xl6CodeScopeLeave(XL6_CodeScope* scope)
{
    XL6_CodeScope* current;
    XL6_CodeLease* lease;
    if (scope == NULL) abort();
    if (scope->lease == NULL) return NULL;
    if (scope->threadId != xrtThreadCurrentId()) abort();
    code_scope_lock_enter();
    for (current = code_scopes; current != NULL; current = current->next)
        if (current->threadId == scope->threadId) break;
    if (current != scope) abort();
    if (scope->previous != NULL) scope->previous->next = scope->next;
    else code_scopes = scope->next;
    if (scope->next != NULL) scope->next->previous = scope->previous;
    lease = scope->lease;
    scope->lease = NULL; scope->previous = NULL; scope->next = NULL; scope->threadId = 0;
    code_scope_lock_leave();
    return lease;
}
void xl6CodeScopeLeave(XL6_CodeScope* scope)
{
    xrtownershipscope mutation = {0}; XL6_CodeLease* lease;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    lease = xl6OwnershipBody_xl6CodeScopeLeave(scope);
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    xl6CodeLeaseRelease(lease);
}
static XL6_CodeLeaseAdmission xl6OwnershipBody_xl6CodeLeaseAcquireScoped(const XL6_CodeOwner* owner, XL6_CodeLease** output)
{
    XL6_CodeScope* scope;
    uint64 threadId;
    if (output == NULL) return XL6_CODE_LEASE_INVALID;
    *output = NULL;
    if (owner == NULL) return XL6_CODE_LEASE_ACQUIRED;
    if (owner->size < sizeof(*owner) || owner->acquire == NULL || owner->release == NULL ||
        (xl6CodeOwnerGraphOps(owner) != NULL && owner->context == NULL)) return XL6_CODE_LEASE_INVALID;
    threadId = xrtThreadCurrentId();
    code_scope_lock_enter();
    for (scope = code_scopes; scope != NULL; scope = scope->next) {
        XL6_CodeLease* lease = scope->lease;
        if (scope->threadId == threadId && lease->context == owner->context &&
            lease->acquire == owner->acquire && lease->release == owner->release &&
            lease->ownerOps == xl6CodeOwnerGraphOps(owner)) {
            *output = xl6CodeLeaseRetain(lease); break;
        }
    }
    code_scope_lock_leave();
    return *output != NULL ? XL6_CODE_LEASE_ACQUIRED : xl6CodeLeaseAcquire(owner, output);
}
XL6_CodeLeaseAdmission xl6CodeLeaseAcquireScoped(const XL6_CodeOwner* owner, XL6_CodeLease** output)
{
    XL6_OWNERSHIP_RETURN(XL6_CodeLeaseAdmission, xl6OwnershipBody_xl6CodeLeaseAcquireScoped(owner, output));
}
static XL6_CodeDrop* xl6OwnershipBody_xl6CodeDropCreate(XL6_CodeLease* parent, XL6_CodeDropProc drop, void* context)
{
    XL6_CodeDrop* record; xerror* prior;
    if (drop == NULL) return NULL;
    prior = xrtTakeError(); record = (XL6_CodeDrop*)xrtMalloc(sizeof(*record));
    if (record != NULL) {
        record->lease = xl6CodeLeaseRetain(parent);
        record->drop = drop; record->context = context;
    }
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    return record;
}
XL6_CodeDrop* xl6CodeDropCreate(XL6_CodeLease* parent, XL6_CodeDropProc drop, void* context)
{
    XL6_OWNERSHIP_RETURN(XL6_CodeDrop*, xl6OwnershipBody_xl6CodeDropCreate(parent, drop, context));
}
static void xl6OwnershipBody_xl6CodeDropAbort(XL6_CodeDrop* record)
{
    XL6_CodeLease* lease;
    if (record == NULL) return;
    lease = record->lease; xrtFree(record); xl6CodeLeaseRelease(lease);
}
void xl6CodeDropAbort(XL6_CodeDrop* record)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_xl6CodeDropAbort(record));
}
static void xl6OwnershipBody_xl6CodeDropInvoke(void* value, void* data)
{
    XL6_CodeDrop* record = (XL6_CodeDrop*)data;
    XL6_CodeLease* lease; XL6_CodeDropProc drop; void* context;
    XL6_CodeScope scope = {0};
    if (record == NULL) abort();
    lease = record->lease; drop = record->drop; context = record->context;
    xl6CodeScopeEnter(&scope, lease);
    xrtFree(record);
    drop(value, context);
    xl6CodeLeaseRelease(lease);
    xl6CodeScopeLeave(&scope);
}
void xl6CodeDropInvoke(void* value, void* data)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_xl6CodeDropInvoke(value, data));
}
