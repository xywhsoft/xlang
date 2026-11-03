#include "src/common/xl6_xrt.h"
#include "xl6_foreign_view.h"
#include "xl6_foreign_view_abi.h"
#include "xl6_foreign_scope_view.h"
#include "xl6_foreign_context.h"
#include "xl6_deferred_cleanup.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Native binding pins are actual owning slots, separate from language value
 * references and temporary physical pins. The root directory is nonowning;
 * its intrusive child list owns one binding pin per still-attached child. */
struct XL6_ForeignView {
    xatomic64 references;
    xatomic64 external;
    xatomic32 closing;
    xatomic32 active;
    xatomic32 nativeLive;
    xatomic32 callbackFailed;
    uint64_t callbackFailures;
    xerror* callbackError;
    uint64 thread;
    const void* claim;
    bool prepared, cleared, binding;
    void* raw;
    void* identity;
    XL6_ForeignView* owner;
    XL6_ForeignView* children;
    XL6_ForeignView* next;
    XL6_ForeignView** previous;
    XL6_ForeignView* rootsNext;
    XL6_ForeignView** rootsPrevious;
    XL6_ForeignView* workNext;
    unsigned work;
    uint64 attempted;
    XL6_CodeLease* code;
    XL6_ForeignViewDescriptorV1 descriptor;
    xmap directory;
    void* watchStorage;
    void* closeStorage;
};
enum { VIEW_CLOSE = 1u, VIEW_DETACH = 2u };
static xatomic32 viewLock;
static XL6_ForeignView* viewRoots;
static XL6_ForeignView* viewWork;
static XL6_ForeignView** viewWorkTail = &viewWork;
static size_t viewCount;
static xatomic64 viewEpoch;
static xatomic64 viewQueued;
static bool viewDeferredDrain(void);
static XL6_DeferredCleanupV1 viewDeferredCleanup={.Run=viewDeferredDrain};
static bool viewCalls(void)
{ return xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_CALLS_V1)!=NULL; }
static bool viewIsDraining(void)
{ return xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_VIEW_DRAINING_V1)!=NULL; }

static void viewMutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void viewMutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void viewLockEnter(void)
{ while (xrtAtomic32Exchange(&viewLock,1,XMEMORY_ACQUIRE)) xrtThreadYield(); }
static void viewLockLeave(void)
{ xrtAtomic32Store(&viewLock,0,XMEMORY_RELEASE); }
static XL6_ForeignView* viewRoot(XL6_ForeignView* view)
{ return view->owner ? view->owner : view; }
static bool viewHold(const void* raw)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!view) return false;
    uint64 refs=xrtAtomic64Load(&view->references,XMEMORY_RELAXED);
    do { if (!refs || refs==UINT64_MAX) return false; }
    while (!xrtAtomic64CompareExchange(&view->references,&refs,refs+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    return true;
}
static void viewDrop(const void*);

/* Caller already holds a mutation and a real reference to the record. */
static void viewEnqueue(XL6_ForeignView* view,unsigned flags)
{
    viewLockEnter();
    if (!view->work) {
        if (!viewHold(view)) abort();
        view->workNext=NULL; *viewWorkTail=view; viewWorkTail=&view->workNext;
        (void)xrtAtomic64FetchAdd(&viewQueued,1,XMEMORY_RELEASE);
    }
    view->work|=flags;
    xl6DeferredCleanupScheduleV1(&viewDeferredCleanup);
    viewLockLeave();
}
static xbytesview viewKey(void* const* raw)
{ return (xbytesview){(const uint8*)raw,sizeof(*raw)}; }

static void viewInvalidate(void* raw)
{
    XL6_ForeignView* view=raw;
    xerror* prior=xrtTakeError();
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    if (view->raw) {
        if (view->owner) (void)xrtMapRemove(&view->owner->directory,viewKey(&view->identity));
        view->raw=NULL;
        xrtAtomic32Store(&view->nativeLive,0,XMEMORY_RELEASE);
        xrtAtomic32Store(&view->closing,1,XMEMORY_RELEASE);
        if (view->binding) viewEnqueue(view,VIEW_DETACH);
    }
    viewMutationEnd(&mutation);
    xrtClearError(); xrtSetErrorTake(prior);
    /* No record/code release while the native notifier remains on stack. */
}
static bool viewAlignment(size_t bytes,size_t alignment)
{
    return alignment && !(alignment&(alignment-1)) && alignment<=_Alignof(max_align_t) &&
        _Alignof(max_align_t)%alignment==0 && (!bytes || bytes%alignment==0);
}
static bool viewDescriptorValid(const XL6_ForeignViewDescriptorV1* d,bool root)
{
    if (!d || d->size!=sizeof(*d) || d->abiTag!=XL6_FOREIGN_VIEW_V1_TAG ||
        !d->family || !*d->family || !d->Destroy || !!d->Watch!=!!d->Unwatch ||
        !viewAlignment(d->watchBytes,d->watchAlignment) ||
        (d->Watch ? !d->watchBytes : d->watchBytes!=0) ||
        !!d->CallBegin!=!!d->CallEnd || !viewAlignment(d->callBytes,d->callAlignment) ||
        (d->CallBegin ? !d->callBytes : d->callBytes!=0)) return false;
    if (root) return !d->Watch || (d->CallBegin && d->Busy);
    return d->Watch && !d->CallBegin && !d->TraceRaw && !d->Busy;
}
static bool viewAppendStorage(size_t* bytes,size_t size,size_t alignment,size_t* offset)
{
    size_t padding=(alignment-(*bytes%alignment))%alignment;
    if (padding>SIZE_MAX-*bytes || size>SIZE_MAX-*bytes-padding) return false;
    *offset=*bytes+padding; *bytes=*offset+size; return true;
}
static XL6_ForeignView* viewAllocate(void* raw,const XL6_ForeignViewDescriptorV1* d,
    const XL6_CodeOwner* codeOwner,bool root)
{
    size_t bytes=sizeof(XL6_ForeignView),watchOffset,callOffset;
    if (!raw || !viewDescriptorValid(d,root)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!viewAppendStorage(&bytes,d->watchBytes,d->watchAlignment,&watchOffset) ||
        !viewAppendStorage(&bytes,root ? d->callBytes : 0,d->callAlignment,&callOffset)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    XL6_ForeignView* view=xrtCalloc(1,bytes);
    if (!view) return NULL;
    view->descriptor=*d;
    view->watchStorage=d->watchBytes ? (char*)view+watchOffset : NULL;
    view->closeStorage=root && d->callBytes ? (char*)view+callOffset : NULL;
    XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquireScoped(codeOwner,&view->code);
    if (admission!=XL6_CODE_LEASE_ACQUIRED) {
        xrtFree(view);
        if (admission!=XL6_CODE_LEASE_RESOURCE_FAILURE) xrtSetErrorKind(XERR_STATE);
        return NULL;
    }
    view->raw=raw; view->identity=raw; view->thread=xrtThreadCurrentId();
    xrtAtomic64Init(&view->references,1); xrtAtomic64Init(&view->external,0);
    xrtAtomic32Init(&view->closing,0); xrtAtomic32Init(&view->active,0);
    xrtAtomic32Init(&view->nativeLive,1);
    xrtAtomic32Init(&view->callbackFailed,0);
    return view;
}
static void viewDiscardUnpublished(XL6_ForeignView* view)
{
    if (!view) return;
    XL6_CodeLease* code=view->code;
    if (!view->owner) xrtMapUnit(&view->directory);
    xrtFree(view); xl6CodeLeaseRelease(code);
}
static XL6_ForeignView* viewCreateRoot(void* raw,const XL6_ForeignViewDescriptorV1* d,
    const XL6_CodeOwner* codeOwner,const XL6_ForeignResultProbeV1* probe)
{
    if(!xl6ForeignContextReadyV1())return NULL;
    XL6_ForeignView* view=viewAllocate(raw,d,codeOwner,true);
    if (!view) return NULL;
    if (!xrtMapInit(&view->directory,sizeof(XL6_ForeignView*))) { viewDiscardUnpublished(view); return NULL; }
    /* Lease admission can invoke host code. The factory observer must still
     * identify a live output before any native Watch dereferences its address. */
    if(probe && !xl6ForeignResultProbeAliveV1(probe)) {
        xrtSetErrorKind(XERR_STATE);viewDiscardUnpublished(view);return NULL;
    }
    if (d->Watch && !d->Watch(raw,view->watchStorage,viewInvalidate,view)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        viewDiscardUnpublished(view); return NULL;
    }
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    if (!viewHold(view)) abort(); /* Native binding's actual self pin. */
    view->binding=true; xrtAtomic64Store(&view->external,1,XMEMORY_RELEASE);
    viewLockEnter();
    view->rootsNext=viewRoots; view->rootsPrevious=&viewRoots;
    if (viewRoots) viewRoots->rootsPrevious=&view->rootsNext;
    viewRoots=view; ++viewCount;
    viewLockLeave(); viewMutationEnd(&mutation);
    return view;
}
XL6_ForeignView* xl6ForeignViewCreateRootV1(void* raw,const XL6_ForeignViewDescriptorV1* descriptor,
    const XL6_CodeOwner* codeOwner)
{return viewCreateRoot(raw,descriptor,codeOwner,NULL);}
XL6_ForeignView* xl6ForeignViewCreateRootProbedV1(void* raw,XL6_ForeignResultProbeV1* probe,const XL6_CodeOwner* owner)
{
    if(!xl6ForeignResultProbeAliveV1(probe) || !probe->root || probe->identity!=raw) {
        xrtSetErrorKind(XERR_STATE);return NULL;
    }
    return viewCreateRoot(raw,&probe->descriptor,owner,probe);
}
XL6_ForeignView* xl6ForeignViewRetainV1(XL6_ForeignView* view)
{
    if (!view) return NULL;
    XL6_ForeignView* root=viewRoot(view);
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    uint64 external=xrtAtomic64Load(&root->external,XMEMORY_RELAXED);
    bool ok=external<UINT64_MAX;
    if (ok) {
        do { if (external==UINT64_MAX) { ok=false; break; } }
        while (!xrtAtomic64CompareExchange(&root->external,&external,external+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
        if (ok && !viewHold(view)) {
            xrtAtomic64FetchSub(&root->external,1,XMEMORY_RELAXED); ok=false;
        }
    }
    viewMutationEnd(&mutation);
    if (!ok) xrtSetErrorKind(XERR_RANGE);
    return ok ? view : NULL;
}
XL6_ForeignView* xl6ForeignViewBorrowChildV1(void* raw,XL6_ForeignView* owner,
    const XL6_ForeignViewDescriptorV1* d,const XL6_CodeOwner* codeOwner)
{
    if (!raw || !owner || !viewDescriptorValid(d,false)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_ForeignView* root=viewRoot(owner);
    if (!root->descriptor.CallBegin) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (root->thread!=xrtThreadCurrentId() || !xl6ForeignContextCallOwnsV1(root) || !xrtAtomic32Load(&root->active,XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&root->closing,XMEMORY_ACQUIRE) || !root->raw) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    XL6_ForeignView* const* found=xrtMapConstGet(&root->directory,viewKey(&raw));
    if (found) {
        if (strcmp((*found)->descriptor.family,d->family)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
        return xl6ForeignViewRetainV1(*found);
    }
    XL6_ForeignView* view=viewAllocate(raw,d,codeOwner,false);
    if (!view) return NULL;
    if(xrtAtomic32Load(&root->closing,XMEMORY_ACQUIRE) || !root->raw) {
        xrtSetErrorKind(XERR_STATE);viewDiscardUnpublished(view);return NULL;
    }
    view->owner=root;
    if (!d->Watch(raw,view->watchStorage,viewInvalidate,view)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        viewDiscardUnpublished(view); return NULL;
    }
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    bool added=xrtMapSet(&root->directory,viewKey(&raw),&view);
    viewMutationEnd(&mutation);
    if (!added) {
        xerror* error=xrtTakeError(); d->Unwatch(view->watchStorage);
        xrtClearError(); xrtSetErrorTake(error);
        viewDiscardUnpublished(view); return NULL;
    }
    viewMutationBegin(&mutation);
    if (!viewHold(root) || !viewHold(view)) abort();
    view->binding=true;
    view->next=root->children; view->previous=&root->children;
    if (view->next) view->next->previous=&view->next;
    root->children=view;
    uint64 external=xrtAtomic64FetchAdd(&root->external,1,XMEMORY_RELAXED);
    if (external==UINT64_MAX) abort();
    viewLockEnter(); ++viewCount; viewLockLeave();
    viewMutationEnd(&mutation);
    return view;
}
XL6_ForeignView* xl6ForeignViewBorrowRootV1(void* raw,XL6_ForeignView* source)
{
    if(!raw || !source){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ForeignView* root=viewRoot(source);
    if(root->thread!=xrtThreadCurrentId() || !xl6ForeignContextCallOwnsV1(root) ||
        !root->raw || raw!=root->raw || root->claim || root->cleared ||
        xrtAtomic32Load(&root->closing,XMEMORY_ACQUIRE)){
        xrtSetErrorKind(XERR_STATE);return NULL;
    }
    return xl6ForeignViewRetainV1(root);
}
static void viewDrop(const void* raw)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!view) return;
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    uint64 refs=xrtAtomic64FetchSub(&view->references,1,XMEMORY_ACQ_REL);
    if (!refs) abort();
    if (refs!=1) { viewMutationEnd(&mutation); return; }
    if (view->binding || view->work || view->children || view->raw) abort();
    XL6_ForeignView* owner=view->owner;
    XL6_CodeLease* code=view->code;
    viewLockEnter();
    bool callbackFailed=xrtAtomic32Load(&view->callbackFailed,XMEMORY_RELAXED)!=0;
    xerror* callbackError=view->callbackError;view->callbackError=NULL;
    xrtAtomic32Store(&view->callbackFailed,0,XMEMORY_RELEASE);
    if (view->rootsPrevious) {
        *view->rootsPrevious=view->rootsNext;
        if (view->rootsNext) view->rootsNext->rootsPrevious=view->rootsPrevious;
    }
    if (!viewCount) abort();
    --viewCount;
    viewLockLeave(); viewMutationEnd(&mutation);
    if(callbackFailed)xl6ErrorScopePropagateFailure((xl6_error_result){false,callbackError});
    if (!owner) xrtMapUnit(&view->directory);
    xrtFree(view);
    viewDrop(owner); xl6CodeLeaseRelease(code);
}
XL6_ForeignView* xl6ForeignViewNativePinV1(XL6_ForeignView* view)
{
    if(!view){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ForeignView* root=viewRoot(view);
    xrtownershipscope mutation={0};viewMutationBegin(&mutation);
    bool ok=root->thread==xrtThreadCurrentId() && !root->claim && !root->cleared &&
        !xrtAtomic32Load(&root->closing,XMEMORY_ACQUIRE) &&
        xrtAtomic32Load(&root->nativeLive,XMEMORY_ACQUIRE) && viewHold(root);
    viewMutationEnd(&mutation);
    if(!ok)xrtSetErrorKind(XERR_STATE);
    return ok?root:NULL;
}
void xl6ForeignViewNativeUnpinV1(XL6_ForeignView* root)
{ viewDrop(root); }
bool xl6ForeignViewReleaseV1(XL6_ForeignView* view)
{
    if (!view) return true;
    XL6_ForeignView* root=viewRoot(view);
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    uint64 external=xrtAtomic64FetchSub(&root->external,1,XMEMORY_ACQ_REL);
    if (!external) abort();
    if (external==1 && xrtAtomic32Load(&root->nativeLive,XMEMORY_ACQUIRE)) {
        xrtAtomic32Store(&root->closing,1,XMEMORY_RELEASE); viewEnqueue(root,VIEW_CLOSE);
    }
    viewMutationEnd(&mutation);
    viewDrop(view);
    if (!viewCalls() && !xl6ForeignCallbackActiveV1() && !viewIsDraining()) return xl6ForeignViewDrainV1();
    return true;
}

static void viewCallPinsEnd(XL6_ForeignView* view,XL6_ForeignView* root)
{
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    if (!xrtAtomic32FetchSub(&root->active,1,XMEMORY_ACQ_REL) || !viewCalls()) abort();
    viewMutationEnd(&mutation);
    viewDrop(view); if (root!=view) viewDrop(root);
}
bool xl6ForeignViewCallBeginV1(XL6_ForeignView* view,XL6_ForeignViewCallV1* scope,
    void* storage,size_t bytes)
{
    if (!view || !scope || scope->self || scope->view || scope->root || scope->raw || scope->nativeStorage ||
        scope->execution.self || scope->execution.older || scope->execution.newer || scope->execution.owner) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_ForeignView* root=viewRoot(view);
    const XL6_ForeignViewDescriptorV1* d=&root->descriptor;
    if (root->thread!=xrtThreadCurrentId() || bytes!=d->callBytes ||
        (d->CallBegin ? !storage || (uintptr_t)storage%d->callAlignment : storage!=NULL)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    if(!xl6ForeignViewThrowIfCallbackFailedV1(root))return false;
    if(!xl6ForeignContextCallEnterV1(&scope->execution,root))return false;
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    bool ok=!root->claim && !view->claim && !root->prepared && !view->cleared && view->raw && root->raw &&
        !xrtAtomic32Load(&root->closing,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&view->closing,XMEMORY_ACQUIRE) &&
        xrtAtomic32Load(&root->active,XMEMORY_RELAXED)!=UINT32_MAX;
    if (ok) {
        ok=viewHold(view);
        if (ok && root!=view && !viewHold(root)) { viewDrop(view); ok=false; }
        if (ok) xrtAtomic32FetchAdd(&root->active,1,XMEMORY_RELAXED);
    }
    viewMutationEnd(&mutation);
    if (!ok) { xl6ForeignContextCallLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE); return false; }
    if (d->CallBegin && !d->CallBegin(root->raw,storage)) {
        viewCallPinsEnd(view,root);xl6ForeignContextCallLeaveV1(&scope->execution);
        if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
        return false;
    }
    scope->self=scope; scope->view=view; scope->root=root;
    scope->raw=view->raw; scope->nativeStorage=storage;
    return true;
}
void xl6ForeignViewRecordCallbackFailureV1(XL6_ForeignView* view,xl6_error_result failure)
{
    if(!view || failure.ok)abort();
    XL6_ForeignView* root=viewRoot(view);xerror* discard=NULL;
    xrtownershipscope mutation={0};viewMutationBegin(&mutation);viewLockEnter();
    if(root->callbackFailures!=UINT64_MAX)++root->callbackFailures;
    if(!xrtAtomic32Load(&root->callbackFailed,XMEMORY_RELAXED)) {
        root->callbackError=failure.error;xrtAtomic32Store(&root->callbackFailed,1,XMEMORY_RELEASE);
    } else discard=failure.error;
    viewLockLeave();viewMutationEnd(&mutation);xrtErrorFree(discard);
}
bool xl6ForeignViewThrowIfCallbackFailedV1(XL6_ForeignView* view)
{
    if(!view){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ForeignView* root=viewRoot(view);
    if(!xrtAtomic32Load(&root->callbackFailed,XMEMORY_ACQUIRE))return true;
    xrtownershipscope mutation={0};viewMutationBegin(&mutation);viewLockEnter();
    bool failed=xrtAtomic32Load(&root->callbackFailed,XMEMORY_RELAXED)!=0;
    xerror* error=root->callbackError;root->callbackError=NULL;
    xrtAtomic32Store(&root->callbackFailed,0,XMEMORY_RELEASE);
    viewLockLeave();viewMutationEnd(&mutation);
    if(failed)xrtSetErrorTake(error);
    return !failed;
}
bool xl6ForeignViewCallbackFailureCountV1(XL6_ForeignView* view,uint64_t* output)
{
    if(!view || !output || (uintptr_t)output%_Alignof(uint64_t)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ForeignView* root=viewRoot(view);
    xrtownershipscope mutation={0};viewMutationBegin(&mutation);viewLockEnter();
    uint64_t count=root->callbackFailures;viewLockLeave();viewMutationEnd(&mutation);
    *output=count;return true;
}
static bool viewPublish(xl6_error_result result)
{
    if (!result.ok) xrtSetErrorTake(result.error);
    return result.ok;
}
bool xl6ForeignViewCallEndV1(XL6_ForeignViewCallV1* scope)
{
    if (!scope) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!scope->self && !scope->view && !scope->root && !scope->raw && !scope->nativeStorage &&
        !scope->execution.self && !scope->execution.older && !scope->execution.newer && !scope->execution.owner) return true;
    if (scope->self!=scope || !scope->view || !scope->root ||
        scope->root->thread!=xrtThreadCurrentId() || !xl6ForeignContextCallContainsV1(&scope->execution)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    xl6ForeignScopeExpireOwnerV1(scope);
    XL6_ForeignView* view=scope->view;
    XL6_ForeignView* root=scope->root;
    void* storage=scope->nativeStorage;
    scope->self=NULL;scope->view=NULL;scope->root=NULL;scope->raw=NULL;scope->nativeStorage=NULL;
    xl6_error_scope errors={0}; if (!xl6ErrorScopeBegin(&errors)) abort();
    if (root->descriptor.CallEnd && (!root->descriptor.CallEnd(storage) || xrtGetError()))
        xl6ErrorScopeCaptureFailure(&errors);
    viewCallPinsEnd(view,root);
    xl6ForeignContextCallLeaveV1(&scope->execution);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
    if (!viewCalls() && !xl6ForeignCallbackActiveV1() && !viewIsDraining() && !xl6ForeignViewDrainV1())
        xl6ErrorScopeCaptureFailure(&errors);
    return viewPublish(xl6ErrorScopeEnd(&errors));
}
static bool viewCloseBody(XL6_ForeignView* view)
{
    if (!view) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_ForeignView* root=viewRoot(view);
    if (root->thread!=xrtThreadCurrentId()) { xrtSetErrorKind(XERR_STATE); return false; }
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    if (view->raw) {
        xrtAtomic32Store(&view->closing,1,XMEMORY_RELEASE); viewEnqueue(view,VIEW_CLOSE);
    }
    viewMutationEnd(&mutation);
    /* An existing native scope already protects the whole C stack. Deliver
     * logical close now; native physical free still waits for its last End.
     * Exclusive resources without native scopes remain queued until idle. */
    if (view->raw && root->descriptor.CallBegin &&
        xrtAtomic32Load(&root->active,XMEMORY_ACQUIRE)) {
        xl6_error_scope errors={0}; if (!xl6ErrorScopeBegin(&errors)) abort();
        bool destroyed=view->descriptor.Destroy(view->raw);
        if(destroyed && !view->descriptor.Watch)viewInvalidate(view);
        if (!destroyed || xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
        return viewPublish(xl6ErrorScopeEnd(&errors));
    }
    if (!viewCalls() && !xl6ForeignCallbackActiveV1() && !viewIsDraining()) return xl6ForeignViewDrainV1();
    return true;
}
bool xl6ForeignViewCloseV1(XL6_ForeignView* view)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return false;
    if(!xl6ForeignViewThrowIfCallbackFailedV1(view))xl6ErrorScopeCaptureFailure(&errors);
    if(!viewCloseBody(view) || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    return viewPublish(xl6ErrorScopeEnd(&errors));
}
static bool viewDestroyNative(XL6_ForeignView* view)
{
    XL6_ForeignView* root=viewRoot(view);
    if (!view->raw) return true;
    if (!root->raw || xrtAtomic32Load(&root->active,XMEMORY_ACQUIRE)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    const XL6_ForeignViewDescriptorV1* d=&root->descriptor;
    XL6_ForeignCallContextV1 execution={0};
    if(!xl6ForeignContextCallEnterV1(&execution,root))return false;
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    xrtAtomic32FetchAdd(&root->active,1,XMEMORY_RELAXED);
    viewMutationEnd(&mutation);
    xl6_error_scope errors={0}; if (!xl6ErrorScopeBegin(&errors)) abort();
    bool begun=!d->CallBegin || d->CallBegin(root->raw,root->closeStorage);
    if (!begun || xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
    if (begun) {
        bool ok=view->descriptor.Destroy(view->raw);
        if (ok && !view->descriptor.Watch) viewInvalidate(view);
        if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
        if (d->CallEnd && (!d->CallEnd(root->closeStorage) || xrtGetError()))
            xl6ErrorScopeCaptureFailure(&errors);
    }
    viewMutationBegin(&mutation);
    if (!xrtAtomic32FetchSub(&root->active,1,XMEMORY_ACQ_REL)) abort();
    viewMutationEnd(&mutation);
    xl6ForeignContextCallLeaveV1(&execution);
    return viewPublish(xl6ErrorScopeEnd(&errors));
}
static void viewDetach(XL6_ForeignView* view)
{
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    bool release=view->binding;
    if (release) {
        if (view->raw) abort();
        view->binding=false;
        if (view->previous) {
            *view->previous=view->next;
            if (view->next) view->next->previous=view->previous;
            view->previous=NULL; view->next=NULL;
        }
    }
    viewMutationEnd(&mutation);
    if (release) viewDrop(view);
}
bool xl6ForeignViewDrainV1(void)
{
    /* Most language cleanup boundaries have never held a foreign view. Do
     * not initialize execution-context storage or acquire the graph lock on
     * that path. Queue publication/removal owns this counter under viewLock. */
    if (!xrtAtomic64Load(&viewQueued,XMEMORY_ACQUIRE)) return true;
    if(!xl6ForeignContextReadyV1())return false;
    if (viewIsDraining() || viewCalls() || xl6ForeignCallbackActiveV1()) return true;
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_VIEW_DRAINING_V1,(void*)(uintptr_t)1))return false;
    uint64 epoch=xrtAtomic64FetchAdd(&viewEpoch,1,XMEMORY_RELAXED)+1;
    if(!epoch)abort(); /* Exhausting distinct traversal epochs is an invariant failure. */
    uint64 thread=xrtThreadCurrentId();
    xl6_error_scope errors={0}; if (!xl6ErrorScopeBegin(&errors)) abort();
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    viewLockEnter();
    size_t budget=viewCount<=SIZE_MAX/2 ? viewCount*2 : SIZE_MAX;
    viewLockLeave(); viewMutationEnd(&mutation);
    /* Bounded by the initial physical population. New user-created work and
     * failed closes remain real owned records for a subsequent checked pass. */
    while (budget--) {
        viewMutationBegin(&mutation); viewLockEnter();
        XL6_ForeignView** slot=&viewWork;
        while (*slot) {
            XL6_ForeignView* item=*slot;
            if (item->thread==thread && !xrtAtomic32Load(&viewRoot(item)->active,XMEMORY_ACQUIRE) &&
                ((item->work&VIEW_DETACH) || item->attempted!=epoch)) break;
            slot=&item->workNext;
        }
        XL6_ForeignView* view=*slot;
        unsigned work=0;
        if (view) {
            *slot=view->workNext;
            if (!view->workNext) viewWorkTail=slot;
            view->workNext=NULL; work=view->work; view->work=0;
            if (!xrtAtomic64FetchSub(&viewQueued,1,XMEMORY_ACQ_REL)) abort();
        }
        viewLockLeave(); viewMutationEnd(&mutation);
        if (!view) break;
        if (work&VIEW_CLOSE) {
            view->attempted=epoch;
            bool closed=viewDestroyNative(view);
            if (!closed || xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
            if (view->raw) {
                viewMutationBegin(&mutation); viewEnqueue(view,VIEW_CLOSE); viewMutationEnd(&mutation);
            }
        }
        if (work&VIEW_DETACH) viewDetach(view);
        viewDrop(view); /* Drop the actual work-queue reference after C returns. */
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&errors);
    }
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_VIEW_DRAINING_V1,NULL))abort();
    return viewPublish(xl6ErrorScopeEnd(&errors));
}

size_t xl6ForeignViewPendingV1(uint64_t thread)
{
    size_t count=0;
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation); viewLockEnter();
    for (XL6_ForeignView* item=viewWork; item; item=item->workNext)
        if (!thread || item->thread==thread) ++count;
    viewLockLeave(); viewMutationEnd(&mutation); return count;
}

static bool viewDeferredDrain(void)
{
    bool ok=xl6ForeignViewDrainV1();
    if (xrtAtomic64Load(&viewQueued,XMEMORY_ACQUIRE))
        xl6DeferredCleanupScheduleV1(&viewDeferredCleanup);
    return ok;
}

static bool viewBusy(const void* raw)
{
    const XL6_ForeignView* root=viewRoot((XL6_ForeignView*)raw);
    /* Freeze excludes resident mutations, not arbitrary native UI writes.
     * Creator-affine raw payloads may only be inspected on their thread. */
    return root->thread!=xrtThreadCurrentId() ||
        xrtAtomic32Load(&root->active,XMEMORY_ACQUIRE)!=0 ||
        (root->raw && root->descriptor.Busy && root->descriptor.Busy(root->raw));
}
static bool viewReadCount(const void* raw,size_t* count)
{
    const XL6_ForeignView* view=raw;
    if (!view || !count || view->cleared || viewBusy(view)) return false;
    uint64 refs=xrtAtomic64Load(&view->references,XMEMORY_ACQUIRE);
    if (!refs || refs>SIZE_MAX) return false;
    *count=(size_t)refs; return true;
}
static const xrtownershipops viewOps;
xrtownershipref xl6ForeignViewOwnershipV1(const XL6_ForeignView* view)
{ return (xrtownershipref){view,&viewOps}; }
static bool viewTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const XL6_ForeignView* view=raw; size_t count;
    if (!visit || !viewReadCount(view,&count)) return false;
    if (view->code && !visit(xl6CodeLeaseOwnership(view->code),context)) return false;
    if (view->callbackError && !visit(xrtErrorOwnership(view->callbackError),context)) return false;
    if (view->owner) return visit(xl6ForeignViewOwnershipV1(view->owner),context);
    if (view->binding && !visit(xl6ForeignViewOwnershipV1(view),context)) return false;
    for (const XL6_ForeignView* child=view->children; child; child=child->next)
        if (!visit(xl6ForeignViewOwnershipV1(child),context)) return false;
    return !view->raw || !view->descriptor.TraceRaw || view->descriptor.TraceRaw(view->raw,visit,context);
}
static const xrtownershipops viewOps={viewReadCount,viewTrace};
static bool viewClaim(const void* raw,const void* token)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!token || view->cleared || viewBusy(view) || (view->claim && view->claim!=token)) return false;
    view->claim=token; return true;
}
static void viewRestore(const void* raw,const void* token)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (view->claim==token && !view->cleared) view->claim=NULL;
}
static bool viewReady(const void* raw)
{
    const XL6_ForeignView* view=raw;
    return !viewBusy(view) && view->prepared;
}
static xrtownershipprepareresult viewPrepare(const void* raw,const void* token)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!token || view->claim!=token || view->cleared) return XRT_OWNERSHIP_PREPARE_FAILED;
    if (viewBusy(view) || viewRoot(view)->thread!=xrtThreadCurrentId()) return XRT_OWNERSHIP_PREPARE_BUSY;
    xrtownershipscope mutation={0}; viewMutationBegin(&mutation);
    view->prepared=true;
    xrtAtomic32Store(&view->closing,1,XMEMORY_RELEASE);
    viewMutationEnd(&mutation);
    return XRT_OWNERSHIP_PREPARE_READY;
}
static bool viewFinalize(const void* raw,const void* token)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!token || view->claim!=token || view->cleared || !viewReady(view)) return false;
    /* Borrowed children do not independently consume a root-owned C payload. */
    return view->owner || xl6ForeignViewCloseV1(view);
}
static void viewClear(const void* raw,const void* token)
{
    XL6_ForeignView* view=(XL6_ForeignView*)raw;
    if (!token || view->claim!=token || view->raw || view->binding || viewBusy(view)) abort();
    view->cleared=true;
}
static bool viewFinish(const void* raw,const void* token)
{
    const XL6_ForeignView* view=raw;
    return token && view->claim==token && view->cleared && !view->raw && !view->binding;
}
static const xrtownershipadapterv1 viewAdapter={sizeof(viewAdapter),viewHold,viewDrop,
    viewClaim,viewRestore,viewFinalize,viewClear,viewFinish};
static const xrtownershippreparationv1 viewPreparation={sizeof(viewPreparation),&viewAdapter,viewReady,viewPrepare};
static const xrtownershipquiescencev1 viewQuiescence={sizeof(viewQuiescence),&viewOps,viewBusy};
static bool viewDomainLoad(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddQuiescenceV1(catalog,&viewQuiescence); }
static bool viewDomainResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)policies;
    if (ref.Ops!=&viewOps || !ref.Data) return false;
    *adapter=&viewAdapter; *preparation=&viewPreparation; *finish=NULL; return true;
}
static bool viewDomainAdmitsRead(xrtownershipref ref)
{ return ref.Data && ref.Ops==&viewOps && !viewBusy(ref.Data); }
const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignViewV1Get(void)
{
    static const XL6_OwnershipDomainV1 domain={sizeof(domain),viewDomainLoad,viewDomainResolve,viewDomainAdmitsRead};
    return &domain;
}
bool xl6ForeignViewDiscoverV1(const XL6_CodeOwner* owner,XL6_ModuleOwnershipVisit visit,void* context)
{
    (void)owner;
    if (!visit) return false;
    /* A root may retain a closure from a different artifact. Include physical
     * anchors rather than inferring incoming edges from a descriptor's code
     * owner. The catalog must admit each discovered node independently. */
    for (const XL6_ForeignView* root=viewRoots; root; root=root->rootsNext)
        if (!visit(xl6ForeignViewOwnershipV1(root),context)) return false;
    return true;
}

bool xl6ForeignViewCopyValueV1(XL6_ForeignView* input,XL6_ForeignView** output)
{
    if(!output || (uintptr_t)output%_Alignof(XL6_ForeignView*) || *output){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    XL6_ForeignView* copy=xl6ForeignViewRetainV1(input);
    if(input && !copy)return false;
    *output=copy;return true;
}
void xl6ForeignViewDropValueV1(XL6_ForeignView* input)
{
    if(!xl6ForeignViewReleaseV1(input))
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
}
bool xl6ForeignViewEqualValueV1(XL6_ForeignView* left,XL6_ForeignView* right)
{
    return left==right;
}
static void resultProbeInvalidate(void* raw)
{
    XL6_ForeignResultProbeV1* probe=raw;
    probe->live=false;
}
bool xl6ForeignResultProbePrepareV1(XL6_ForeignResultProbeV1* probe,
    const XL6_ForeignViewDescriptorV1* descriptor,bool root,void* storage,size_t bytes)
{
    if(!probe || probe->self || probe->thread || probe->storage || probe->identity || probe->root || probe->live || probe->observed ||
        !viewDescriptorValid(descriptor,root) || bytes!=descriptor->watchBytes ||
        (bytes ? !storage || (uintptr_t)storage%descriptor->watchAlignment : storage!=NULL)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    probe->descriptor=*descriptor;probe->storage=storage;probe->root=root;probe->thread=xrtThreadCurrentId();
    probe->self=probe;return true;
}
bool xl6ForeignResultProbeObserveV1(XL6_ForeignResultProbeV1* probe,void* raw)
{
    if(!probe || probe->self!=probe || probe->thread!=xrtThreadCurrentId() || probe->observed){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    probe->observed=true;probe->identity=raw;
    if(!raw)return true;
    probe->live=true;
    if(probe->descriptor.Watch && !probe->descriptor.Watch(raw,probe->storage,resultProbeInvalidate,probe)){
        probe->live=false;return false;
    }
    return true;
}
bool xl6ForeignResultProbeAliveV1(const XL6_ForeignResultProbeV1* probe)
{
    return probe && probe->self==probe && probe->observed && probe->thread==xrtThreadCurrentId() &&
        probe->live;
}
bool xl6ForeignResultProbeEndV1(XL6_ForeignResultProbeV1* probe)
{
    if(!probe){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!probe->self && !probe->thread && !probe->storage && !probe->identity && !probe->root && !probe->observed && !probe->live)return true;
    if(probe->self!=probe || probe->thread!=xrtThreadCurrentId()){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    if(probe->descriptor.Unwatch)probe->descriptor.Unwatch(probe->storage);
    *probe=(XL6_ForeignResultProbeV1){0};return true;
}
