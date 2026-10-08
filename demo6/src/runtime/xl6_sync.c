#define XRT_MODULE_ONCE
#define XRT_MODULE_THREAD
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_sync_internal.h"
#include "release/include/xl_sync_retirement.h"
#include <limits.h>
#include <stdlib.h>

typedef XL6_SyncOwner SyncOwner;
static _Atomic(uint32_t) registryGate;
static xonce registryOnce=XRT_ONCE_INIT;
static SyncOwner* pendingHead;
static uint64_t nextToken;
static const xvaluehandleops handleOps;
static bool registryInit(void* context)
{ (void)context; atomic_init(&registryGate,0); return true; }
static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void gateLock(_Atomic(uint32_t)* gate)
{
    uint32_t available=0;
    while (!atomic_compare_exchange_weak_explicit(gate,&available,1,memory_order_acq_rel,memory_order_acquire)) {
        available=0; xrtThreadYield();
    }
}
static void gateUnlock(_Atomic(uint32_t)* gate) { atomic_store_explicit(gate,0,memory_order_release); }
/* Every metadata gate is inside a world mutation, in this order. A global
 * freeze therefore observes ordinary fields coherently without locking them.
 * Registry operations never hold its gate while obtaining an owner's gate.
 * Activity/native Unit runs outside both gates and its OWN world scope. */
static SyncOwner* require(const xvalue* value,uint64_t identity,unsigned int kind)
{
    void* raw=NULL; void* context=NULL; const xvaluehandleops* actual=NULL;
    if (!value || !identity || kind<1 || kind>3 || xrtValueType(value)!=XVALUE_HANDLE ||
        xrtValueTypeId(value)!=identity || !xrtValueGetHandle(value,&raw,&actual,&context) ||
        actual!=&handleOps || context || !raw) return NULL;
    SyncOwner* owner=raw;
    return owner->identity==identity && owner->kind==kind ? owner : NULL;
}
bool xl6SyncAdmitsV1(const xvalue* value,uint64_t identity,unsigned int kind)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    SyncOwner* owner=require(value,identity,kind); bool ok=false;
    if (owner) {
        gateLock(&owner->gate);
        ok=owner->initialized && !owner->cleared && !owner->pending && !owner->finishing;
        gateUnlock(&owner->gate);
    }
    mutationEnd(&mutation); return ok;
}
/* Caller owns the metadata gate. The intrusive record allocates nothing and
 * is itself one real native reference. Token exhaustion is an explicit
 * bounded-state invariant, never wraparound/ABA or loss of the failed owner. */
static void enlist(SyncOwner* owner,unsigned int errorKind)
{
    gateLock(&registryGate);
    owner->errorKind=errorKind;
    if (owner->pending) { gateUnlock(&registryGate); return; }
    if (nextToken==UINT64_MAX || xrtRefRetain(&owner->refs)<=0) abort();
    owner->token=++nextToken; owner->pending=true;
    owner->next=pendingHead; pendingHead=owner;
    gateUnlock(&registryGate);
}
/* Caller owns the metadata gate and a separate real lifetime pin. */
static void delist(SyncOwner* owner)
{
    if (!owner->pending) return;
    gateLock(&registryGate);
    SyncOwner** link=&pendingHead;
    while (*link && *link!=owner) link=&(*link)->next;
    if (!*link) abort();
    *link=owner->next; owner->next=NULL; owner->pending=false; owner->token=0;
    if (xrtRefRelease(&owner->refs)<=0) abort();
    gateUnlock(&registryGate);
}
static bool retire(SyncOwner* owner)
{
    xl6_error_scope work={0};
    if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    bool ok=owner->unit(owner);
    unsigned int kind=!ok && xrtGetError() ? (unsigned int)xrtErrorKind(xrtGetError()) : XERR_STATE;
    if (!ok) xl6ErrorScopeCaptureFailure(&work);
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
    if (!owner->finishing || owner->active) abort();
    if (ok) { owner->initialized=false; delist(owner); }
    else enlist(owner,kind);
    owner->finishing=false;
    gateUnlock(&owner->gate); mutationEnd(&mutation);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    if (!ok) xl6ErrorScopePropagateFailure(result);
    else xrtErrorFree(result.error);
    return ok;
}
void xl6SyncOwnerDropNative(void* raw,void* context)
{
    if (context) abort();
    SyncOwner* owner=raw; if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
    int32_t refs=xrtRefRelease(&owner->refs);
    if (refs<0) abort();
    if (refs) { gateUnlock(&owner->gate); mutationEnd(&mutation); return; }
    if (owner->active || owner->pending || owner->finishing) abort();
    bool initialized=owner->initialized;
    /* A private transition pin keeps RC positive while the physical Unit may
     * refuse. On refusal the registry takes its OWN pin before this is dropped. */
    if (initialized) { owner->refs=1; owner->finishing=true; }
    gateUnlock(&owner->gate); mutationEnd(&mutation);
    if (!initialized) { xrtFree(owner); return; }
    (void)retire(owner);
    /* On refusal, registry is the real remaining owner. On success the
     * transition pin is the final reference of an already-uninitialized node. */
    xl6SyncOwnerDropNative(owner,NULL);
}
static bool clone(void* raw,void** output,void* context)
{
    if (context) abort();
    SyncOwner* owner=raw; bool ok=false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (owner) {
        gateLock(&owner->gate);
        ok=output && owner->initialized && !owner->cleared && !owner->pending &&
            !owner->finishing && xrtRefRetain(&owner->refs)>0;
        if (ok) *output=owner;
        gateUnlock(&owner->gate);
    }
    if (!ok) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return ok;
}
static const xvaluehandleops handleOps={clone,xl6SyncOwnerDropNative,NULL,NULL};
static bool count(const void* raw,size_t* result)
{
    const SyncOwner* owner=raw;
    if (!owner || !result || owner->refs<=0 || owner->active || owner->finishing ||
        !owner->initialized || owner->cleared || owner->pending) return false;
    *result=(size_t)owner->refs; return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{ (void)context; size_t refs; return visit && count(raw,&refs); }
static const xrtownershipops ownerOps={count,trace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    if (!visit || !xrtValueGetHandle(value,&raw,&actual,&user) || !raw || user) return false;
    return actual==&handleOps && visit((xrtownershipref){raw,&ownerOps},context);
}
static bool hold(const void* raw)
{ SyncOwner* owner=(SyncOwner*)raw; return owner && owner->refs>0 && xrtRefRetain(&owner->refs)>0; }
static void release(const void* raw) { xl6SyncOwnerDropNative((void*)raw,NULL); }
static bool claim(const void* raw,const void* token)
{
    SyncOwner* owner=(SyncOwner*)raw;
    if (!token || owner->active || owner->finishing || owner->pending || owner->cleared ||
        (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void restore(const void* raw,const void* token)
{
    SyncOwner* owner=(SyncOwner*)raw;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    SyncOwner* owner=(SyncOwner*)raw;
    if (!token || owner->claim!=token || owner->active || owner->finishing || owner->cleared) abort();
    owner->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    SyncOwner* owner=(SyncOwner*)raw;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
    if (!token || owner->claim!=token || !owner->cleared) abort();
    bool ok=!owner->initialized;
    bool busy=owner->active || owner->finishing;
    if (!ok && !busy) owner->finishing=true;
    gateUnlock(&owner->gate); mutationEnd(&mutation);
    if (ok) return true;
    if (busy) { xrtSetErrorKind(XERR_STATE); return false; }
    return retire(owner);
}
const xrtownershipadapterv1* xl6SyncOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,release,claim,restore,NULL,clear,finish};
    size_t refs;
    if (ref.Data && ref.Ops==&ownerOps) return count(ref.Data,&refs) ? &adapter : NULL;
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
xvalue* xl6SyncOwnerWrapTakeNative(SyncOwner** owner)
{
    if (!xrtOnce(&registryOnce,registryInit,NULL)) return NULL;
    uint64_t identity=(*owner)->identity;
    xvalue* value=xrtValueHandleTake((void**)owner,&handleOps,NULL);
    if (value && (!xrtValueTypeIdBind(value,identity) ||
        !xrtValueHandleOwnershipBindPhased(value,handleTrace))) { xrtValueRelease(value); return NULL; }
    return value;
}
static SyncOwner* begin(const xvalue* value,uint64_t identity,unsigned int kind,bool unlock,bool recoveryOnly)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    SyncOwner* owner=require(value,identity,kind);
    if (owner) {
        gateLock(&owner->gate);
        bool ok=owner->initialized && !owner->finishing && owner->active<SIZE_MAX &&
            (unlock || (!owner->cleared && !owner->pending)) && (!recoveryOnly || owner->pending) && xrtRefRetain(&owner->refs)>0;
        if (ok) ++owner->active;
        gateUnlock(&owner->gate); if (!ok) owner=NULL;
    }
    mutationEnd(&mutation); return owner;
}
void xl6SyncOwnerEndNative(SyncOwner* owner)
{
    if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
    if (!owner->active) abort();
    --owner->active;
    gateUnlock(&owner->gate); mutationEnd(&mutation); xl6SyncOwnerDropNative(owner,NULL);
}
SyncOwner* xl6SyncOwnerBeginNative(const xvalue* value,uint64_t identity,unsigned int kind,bool unlock)
{ return begin(value,identity,kind,unlock,false); }

size_t xl6SyncRetirementSnapshotV1(XL6_SyncRetirementV1* rows,size_t capacity)
{
    if (!rows && capacity) return 0;
    if (!xrtOnce(&registryOnce,registryInit,NULL)) return 0;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&registryGate);
    size_t count=0;
    for (SyncOwner* owner=pendingHead;owner;owner=owner->next) {
        if (count<capacity) rows[count]=(XL6_SyncRetirementV1){owner->token,owner->identity,owner->kind,owner->errorKind};
        if (count==SIZE_MAX) abort();
        ++count;
    }
    gateUnlock(&registryGate); mutationEnd(&mutation); return count;
}
/* Lookup pins the actual record before releasing its list gate. Per-owner
 * metadata is inspected only after that, avoiding list/owner lock inversion. */
static SyncOwner* lookup(uint64_t token)
{
    if (!token || !xrtOnce(&registryOnce,registryInit,NULL)) return NULL;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&registryGate);
    SyncOwner* owner=pendingHead;
    while (owner && owner->token!=token) owner=owner->next;
    if (owner && xrtRefRetain(&owner->refs)<=0) abort();
    gateUnlock(&registryGate); mutationEnd(&mutation); return owner;
}
xvalue* xl6SyncRetirementAcquireV1(uint64_t token)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    SyncOwner* owner=lookup(token); xvalue* value=NULL;
    if (owner) {
        xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
        bool ok=owner->pending && owner->token==token && owner->initialized && !owner->finishing &&
            owner->active<SIZE_MAX && xrtRefRetain(&owner->refs)>0;
        if (ok) ++owner->active;
        gateUnlock(&owner->gate); mutationEnd(&mutation);
        if (ok) {
            SyncOwner* activity=owner;
            value=xl6SyncOwnerWrapTakeNative(&owner);
            xl6SyncOwnerEndNative(activity);
        }
    }
    if (!value) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&work); }
    if (owner) xl6SyncOwnerDropNative(owner,NULL);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    if (!value) xl6ErrorScopePropagateFailure(result);
    else xrtErrorFree(result.error);
    return value;
}
bool xl6SyncRetirementRetryV1(uint64_t token)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    SyncOwner* owner=lookup(token); bool ok=false;
    if (owner) {
        xrtownershipscope mutation={0}; mutationBegin(&mutation); gateLock(&owner->gate);
        bool ready=owner->pending && owner->token==token && owner->initialized && !owner->active && !owner->finishing;
        if (ready) owner->finishing=true;
        gateUnlock(&owner->gate); mutationEnd(&mutation);
        if (ready) ok=retire(owner);
    }
    if (!ok) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); xl6ErrorScopeCaptureFailure(&work); }
    if (owner) xl6SyncOwnerDropNative(owner,NULL);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    if (!ok) xl6ErrorScopePropagateFailure(result);
    else xrtErrorFree(result.error);
    return ok;
}

/* Recovery is maintenance during an already failing cleanup. Physical
 * success is independent of the inherited escaping outcome. On failure,
 * keep the first primary (including an allocated-error-free failure bit);
 * on success, do not turn that existing outcome into a new API refusal. */
static int checkedEnd(xl6_error_scope* scope,bool ok)
{
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!ok) xl6ErrorScopePropagateFailure(result);
    else xrtErrorFree(result.error);
    return ok ? 1 : 0;
}

int xlSyncRetirementSnapshotCheckedV1(XL_SyncRetirementV1* rows,size_t capacity,size_t* total)
{
    if (total) *total=0;
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    bool ok=total && (rows || !capacity);
    if (!ok) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!(ok=xrtOnce(&registryOnce,registryInit,NULL))) xrtSetErrorKind(XERR_STATE);
    else *total=xl6SyncRetirementSnapshotV1(rows,capacity);
    if (!ok) xl6ErrorScopeCaptureFailure(&work);
    return checkedEnd(&work,ok);
}
int xlSyncRetirementAcquireCheckedV1(uint64_t token,xvalue** output)
{
    if (output) *output=NULL;
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else *output=xl6SyncRetirementAcquireV1(token);
    if (!output || !*output) xl6ErrorScopeCaptureFailure(&work);
    return checkedEnd(&work,output && *output);
}
int xlSyncRetirementUnlockCheckedV1(const xvalue* value,unsigned int mode)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBeginCleanup(&work,false)) abort();
    unsigned int kind=mode==XL_SYNC_UNLOCK_MUTEX_V1 ? XL6_SYNC_MUTEX_V1 :
        (mode==XL_SYNC_UNLOCK_READ_V1 || mode==XL_SYNC_UNLOCK_WRITE_V1) ? XL6_SYNC_RWLOCK_V1 : 0;
    SyncOwner* owner=begin(value,xrtValueTypeId(value),kind,true,true);
    bool ok=owner && owner->unlock && owner->unlock(owner,mode);
    if (!ok) {
        if (!xrtGetError()) xrtSetErrorKind(kind ? XERR_STATE : XERR_ARGUMENT);
        xl6ErrorScopeCaptureFailure(&work);
    }
    xl6SyncOwnerEndNative(owner);
    return checkedEnd(&work,ok);
}
int xlSyncRetirementRetryCheckedV1(uint64_t token)
{ return xl6SyncRetirementRetryV1(token) ? 1 : 0; }
