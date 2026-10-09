#define XRT_MODULE_THREAD
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_thread.h"
#include "src/api/xl_foreign_error.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <limits.h>

typedef struct ThreadOwner {
    volatile int32_t refs;
    _Atomic(uint32_t) gate;
    uint64_t identity;
    xthread* thread;
    XL6_Closure* callable;
    size_t active;
    const void* claim;
    bool creating, workerActive, cleared;
} ThreadOwner;
static const xvaluehandleops handleOps;
static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void lock(ThreadOwner* owner)
{
    uint32_t available=0;
    while (!atomic_compare_exchange_weak_explicit(&owner->gate,&available,1,memory_order_acq_rel,memory_order_acquire)) {
        available=0; xrtThreadYield();
    }
}
static void unlock(ThreadOwner* owner)
{ atomic_store_explicit(&owner->gate,0,memory_order_release); }
static ThreadOwner* require(const xvalue* value,uint64_t identity)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!value || !identity || xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=identity ||
        !xrtValueGetHandle(value,&raw,&ops,&user) || ops!=&handleOps || user || !raw) return NULL;
    ThreadOwner* owner=raw;
    return owner->identity==identity ? owner : NULL;
}
bool xl6ThreadAdmitsV1(const xvalue* value,uint64_t identity)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ThreadOwner* owner=require(value,identity); bool ok=false;
    if (owner) { lock(owner); ok=owner->thread && !owner->creating && !owner->cleared; unlock(owner); }
    mutationEnd(&mutation); return ok;
}
/* A last shell's wait gets its own transition pin before dropping gates.
 * Worker release never waits for itself. Native method pins keep this branch
 * from joining while another method is still in flight. */
static void releaseOwner(ThreadOwner* owner,bool worker)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation); lock(owner);
    int32_t refs=xrtRefRelease(&owner->refs); if (refs<0) abort();
    bool wait=!worker && !owner->creating && !owner->active && owner->thread &&
        (!refs || (refs==1 && owner->workerActive));
    bool freeOwner=!refs;
    if (wait) { owner->refs=refs+1; freeOwner=false; }
    unlock(owner); mutationEnd(&mutation);
    if (wait) {
        xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
        /* The caller thread identity is not substituted; self-release lets
           the genuine worker pin finish the callback and remaining cleanup. */
        if (xrtThreadId(owner->thread)!=xrtThreadCurrentId()) (void)xrtThreadWait(owner->thread);
        xl6_error_result result=xl6ErrorScopeEnd(&cleanup); xrtErrorFree(result.error);
        releaseOwner(owner,true); return;
    }
    if (freeOwner) {
        if (owner->active || owner->workerActive || (owner->claim && !owner->cleared)) abort();
        xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
        if (owner->callable) xl6ClosureRuntimeV1Get()->Release(owner->callable);
        if (owner->thread) xrtThreadDestroy(owner->thread);
        xrtFree(owner);
        xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
        if (!result.ok) xl6ErrorScopePropagateFailure(result); else xrtErrorFree(result.error);
    }
}
static void drop(void* raw,void* user)
{ if (user) abort(); if (raw) releaseOwner(raw,false); }
static bool clone(void* raw,void** output,void* user)
{
    if (user) abort();
    ThreadOwner* owner=raw; bool ok=false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (owner) {
        lock(owner); ok=output && owner->thread && !owner->creating && !owner->cleared && xrtRefRetain(&owner->refs)>0;
        if (ok) *output=owner;
        unlock(owner);
    }
    if (!ok) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return ok;
}
static const xvaluehandleops handleOps={clone,drop,NULL,NULL};
static bool count(const void* raw,size_t* result)
{
    const ThreadOwner* owner=raw;
    if (!owner || !result || owner->refs<=0 || owner->creating || owner->active || !owner->thread || owner->cleared) return false;
    *result=(size_t)owner->refs; return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const ThreadOwner* owner=raw; size_t refs;
    if (!visit || !count(raw,&refs)) return false;
    return !owner->callable || visit(xl6ClosureRuntimeV1Get()->Ownership(owner->callable),context);
}
static const xrtownershipops ownerOps={count,trace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user && ops==&handleOps &&
        visit((xrtownershipref){raw,&ownerOps},context);
}
static bool hold(const void* raw)
{ ThreadOwner* owner=(ThreadOwner*)raw; return owner && owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0; }
static void unpin(const void* raw) { releaseOwner((ThreadOwner*)raw,false); }
static bool claim(const void* raw,const void* token)
{
    ThreadOwner* owner=(ThreadOwner*)raw;
    if (!token || owner->creating || owner->workerActive || owner->active || owner->cleared ||
        (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void restore(const void* raw,const void* token)
{
    ThreadOwner* owner=(ThreadOwner*)raw;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    ThreadOwner* owner=(ThreadOwner*)raw;
    if (!token || owner->claim!=token || owner->workerActive || owner->active || owner->callable || owner->cleared) abort();
    owner->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    ThreadOwner* owner=(ThreadOwner*)raw;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); lock(owner);
    if (!token || owner->claim!=token || !owner->cleared || owner->active || owner->workerActive) abort();
    /* The real worker pin makes a running callback externally rooted. There
       can be no callable duty by the time this node is legally committed. */
    xthread* thread=owner->thread; owner->thread=NULL;
    unlock(owner); mutationEnd(&mutation);
    if (thread) xrtThreadDestroy(thread);
    return true;
}
const xrtownershipadapterv1* xl6ThreadOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,unpin,claim,restore,NULL,clear,finish};
    if (!ref.Data || !ref.Ops) return NULL;
    if (ref.Ops==&ownerOps) { size_t refs; return count(ref.Data,&refs) ? &adapter : NULL; }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
static int32_t entry(void* raw)
{
    ThreadOwner* owner=raw; XL6_Closure* callable=owner->callable;
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    xl6_error_scope work={0}; bool begun=xl6ErrorScopeBegin(&work);
    int64_t value=1; bool ok=false;
    if (begun) {
        XL6_ClosureScope scope={0}; runtime->Enter(&scope,callable);
        ok=((bool (*)(void*,int64_t*))callable->invoke)(callable->environment,&value);
        if (!ok) xl6ErrorScopeCaptureFailure(&work);
        runtime->Leave(&scope);
    }
    /* Publish removal of the actual owning edge before arbitrary captures
       and their source leases are released, with no metadata gate held. */
    xrtownershipscope mutation={0}; mutationBegin(&mutation); lock(owner);
    owner->callable=NULL; owner->workerActive=false; unlock(owner); mutationEnd(&mutation);
    runtime->Release(callable); releaseOwner(owner,true);
    if (begun) {
        xl6_error_result result=xl6ErrorScopeEnd(&work);
        ok=ok && result.ok; xrtErrorFree(result.error);
    }
    xrtClearError();
    uint32_t bits=(uint32_t)value;
    return !ok ? 1 : bits<=UINT32_C(0x7FFFFFFF) ? (int32_t)bits : -1-(int32_t)(UINT32_MAX-bits);
}
xvalue* xl6ThreadCreateV1(XL6_Closure* callable,uint64_t identity)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort();
    ThreadOwner* owner=NULL; xvalue* value=NULL;
    if (!identity || !callable || !callable->invoke ||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(callable))) xrtSetErrorKind(XERR_ARGUMENT);
    else if ((owner=xrtCalloc(1,sizeof(*owner)))!=NULL) {
        owner->refs=1; owner->identity=identity; owner->creating=true; atomic_init(&owner->gate,0);
        owner->callable=xl6ClosureRuntimeV1Get()->Retain(callable);
        if (owner->callable) {
            ThreadOwner* raw=owner; value=xrtValueHandleTake((void**)&raw,&handleOps,NULL);
            if (!value) owner=raw;
            else if (!xrtValueTypeIdBind(value,identity) || !xrtValueHandleOwnershipBindPhased(value,handleTrace)) {
                xl6ErrorScopeCaptureFailure(&work); owner=NULL; xrtValueRelease(value); value=NULL;
            } else {
                xrtownershipscope mutation={0}; mutationBegin(&mutation); lock(owner);
                owner->workerActive=true; if (xrtRefRetain(&owner->refs)<=0) abort();
                unlock(owner); mutationEnd(&mutation);
                xthread* thread=xrtThreadCreate(entry,owner,0);
                mutationBegin(&mutation); lock(owner); owner->thread=thread; owner->creating=false;
                if (!thread) owner->workerActive=false;
                unlock(owner); mutationEnd(&mutation);
                if (!thread) {
                    xl6ErrorScopeCaptureFailure(&work); releaseOwner(owner,true);
                    xrtValueRelease(value); value=NULL;
                }
                owner=NULL;
            }
        }
    }
    if (!value) xl6ErrorScopeCaptureFailure(&work);
    if (owner) releaseOwner(owner,true);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); xrtValueRelease(value); return NULL; }
    return value;
}
static ThreadOwner* begin(const xvalue* value,uint64_t identity)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ThreadOwner* owner=require(value,identity); bool ok=false;
    if (owner) {
        lock(owner); ok=owner->thread && !owner->creating && !owner->cleared && !owner->claim &&
            owner->active!=SIZE_MAX && xrtRefRetain(&owner->refs)>0;
        if (ok) ++owner->active;
        unlock(owner);
    }
    if (!ok) { owner=NULL; xrtSetErrorKind(XERR_STATE); }
    mutationEnd(&mutation); return owner;
}
static void end(ThreadOwner* owner)
{
    if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation); lock(owner);
    if (!owner->active) abort();
    --owner->active; unlock(owner); mutationEnd(&mutation);
    releaseOwner(owner,false);
}
#define METHOD(Name,Type,Default,Operation) \
Type xl6Thread##Name##V1(const xvalue* value,uint64_t identity) { \
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort(); \
    ThreadOwner* owner=begin(value,identity); Type result=owner ? (Operation) : (Default); \
    end(owner); xl6_error_result outcome=xl6ErrorScopeEnd(&work); xrtErrorFree(outcome.error); return result; \
}
METHOD(Wait,bool,false,xrtThreadWait(owner->thread)==XWAIT_OK)
METHOD(ExitCode,int64_t,0,xrtThreadExitCode(owner->thread))
METHOD(State,int64_t,0,xrtThreadState(owner->thread))
METHOD(Stop,bool,false,xrtThreadStop(owner->thread))
bool xl6ThreadWaitTimeoutV1(const xvalue* value,uint64_t identity,int64_t timeout)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort();
    ThreadOwner* owner=begin(value,identity);
    uint64_t micros=timeout<0 || (uint64_t)timeout>UINT64_MAX/1000 ? UINT64_MAX : (uint64_t)timeout*1000;
    bool ok=owner && xrtThreadWaitFor(owner->thread,micros)==XWAIT_OK;
    end(owner); xl6_error_result outcome=xl6ErrorScopeEnd(&work); xrtErrorFree(outcome.error); return ok;
}
