#include "xl6_tls_session_internal.h"
#include <stdlib.h>
static void mutationBegin(xrtownershipscope* scope)
{if(!xrtOwnershipMutationBegin(scope))abort();}
static void mutationEnd(xrtownershipscope* scope)
{if(!xrtOwnershipScopeEnd(scope))abort();}
static void destroyNative(xtlssession* native)
{
    xerror* prior=xrtTakeError();xrtTlsSessionDestroy(native);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
XL6_TlsSessionV1* xl6TlsSessionTakeInternalV1(xtlssession* native,const XL6_TlsSessionRoleOpsV1* role)
{return xl6TlsSessionTakeSelectorInternalV1(native,role,(XL6_TlsSelectorV1){0});}
XL6_TlsSessionV1* xl6TlsSessionTakeSelectorInternalV1(xtlssession* native,const XL6_TlsSessionRoleOpsV1* role,XL6_TlsSelectorV1 selector)
{return xl6TlsSessionTakeCallbacksInternalV1(native,role,selector,(XL6_TlsResumeLookupV1){0});}
XL6_TlsSessionV1* xl6TlsSessionTakeCallbacksInternalV1(xtlssession* native,const XL6_TlsSessionRoleOpsV1* role,XL6_TlsSelectorV1 selector,XL6_TlsResumeLookupV1 lookup)
{
    if(!native)return NULL;
    if(xrtGetError()){destroyNative(native);return NULL;}
    if(!role||!role->drive){xl6TlsFailV1(XERR_ARGUMENT,"TLS session needs permanent resident role operations");destroyNative(native);return NULL;}
    XL6_TlsSessionV1* self=xrtCalloc(1,sizeof(*self));
    if(!self){destroyNative(native);return NULL;}
    self->refs=1;self->native=native;self->role=role;self->selector=xl6TlsSelectorRefV1(selector);
    self->resumeLookup=xl6TlsResumeLookupRefV1(lookup);
    if((selector.data&&!self->selector.data)||(lookup.data&&!self->resumeLookup.data)){
        xl6TlsOutcomeV1(false);xl6TlsSessionDropInternalV1(self);return NULL;}
    return self;
}
bool xl6TlsSessionHoldInternalV1(const void* raw)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;
    if(!self)return false;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    bool held=!self->cleared&&!self->claim&&xrtRefRetain(&self->refs)>0;
    mutationEnd(&mutation);return held;
}
void xl6TlsSessionDropInternalV1(const void* raw)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;if(!self)return;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    int32 remaining=xrtRefRelease(&self->refs);if(remaining<0)abort();
    mutationEnd(&mutation);
    if(!remaining){
        if(xrtAtomic32Load(&self->gate,XMEMORY_ACQUIRE))abort();
        destroyNative(self->native);xl6TlsSelectorDropV1(&self->selector);xl6TlsResumeLookupDropV1(&self->resumeLookup);xrtFree(self);
    }
}
bool xl6TlsSessionEnterInternalV1(XL6_TlsSessionV1* self,bool requireNative,XL6_TlsSessionScopeV1* scope)
{
    if(xrtGetError())return false;
    if(!self||!scope||scope->owner)return xl6TlsFailV1(XERR_ARGUMENT,"missing TLS session entry");
    xrtownershipscope mutation={0};mutationBegin(&mutation);uint32 available=0;
    bool admitted=!self->cleared&&!self->claim&&
        xrtAtomic32CompareExchange(&self->gate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED);
    /* Native is mutable outside Mutation while the gate is held. Read it ONLY
     * after acquiring that gate; rejection must not race an active dispose. */
    bool valid=admitted&&(!requireNative||self->native);
    bool held=valid&&xrtRefRetain(&self->refs)>0;
    if(admitted&&!held)xrtAtomic32Store(&self->gate,0,XMEMORY_RELEASE);
    if(held)scope->owner=self;
    mutationEnd(&mutation);
    return held||xl6TlsFailV1(valid?XERR_RANGE:XERR_STATE,
        valid?"TLS session reference limit exceeded":"TLS session is disposed, claimed or already being driven");
}
void xl6TlsSessionLeaveInternalV1(XL6_TlsSessionScopeV1* scope)
{
    if(!scope||!scope->owner)abort();
    XL6_TlsSessionV1* self=scope->owner;scope->owner=NULL;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    if(xrtAtomic32Load(&self->gate,XMEMORY_ACQUIRE)!=1)abort();
    xrtAtomic32Store(&self->gate,0,XMEMORY_RELEASE);mutationEnd(&mutation);
    xl6TlsSessionDropInternalV1(self);
}
void xl6TlsSessionDestroyV1(void* raw){xl6TlsSessionDropInternalV1(raw);}
void xl6TlsSessionDisposeV1(void* raw)
{
    XL6_TlsSessionScopeV1 scope={0};XL6_TlsSessionV1* self=raw;
    if(!xl6TlsSessionEnterInternalV1(self,false,&scope))return;
    /* A live entry is visible as Busy throughout detached native cleanup.
     * No graph callback may read the changing native buffer queues. */
    xtlssession* native=self->native;self->native=NULL;destroyNative(native);
    xl6TlsSelectorDropV1(&self->selector);
    xl6TlsResumeLookupDropV1(&self->resumeLookup);
    xl6TlsSessionLeaveInternalV1(&scope);
}
bool xl6TlsSessionDisposedV1(void* raw)
{
    XL6_TlsSessionScopeV1 scope={0};XL6_TlsSessionV1* self=raw;
    if(!xl6TlsSessionEnterInternalV1(self,false,&scope))return false;
    bool disposed=!self->native;xl6TlsSessionLeaveInternalV1(&scope);return disposed;
}
static bool busy(const void* raw)
{return raw&&xrtAtomic32Load(&((const XL6_TlsSessionV1*)raw)->gate,XMEMORY_ACQUIRE)!=0;}
static bool count(const void* raw,size_t* result)
{
    const XL6_TlsSessionV1* self=raw;
    if(!self||!result||self->cleared||busy(self)||self->refs<=0)return false;
    *result=(size_t)self->refs;return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const XL6_TlsSessionV1* self=raw;size_t references;
    return visit&&count(self,&references)&&(!self->native||
        xrtTlsSessionBufferOwnershipTrace(self->native,visit,context))&&
        (!self->selector.data||visit(self->selector.ops->Ownership(self->selector.data),context))&&
        (!self->resumeLookup.data||visit(self->resumeLookup.ops->Ownership(self->resumeLookup.data),context));
}
static const xrtownershipops ops={count,trace};
xrtownershipref xl6TlsSessionOwnershipV1(const void* raw)
{return (xrtownershipref){raw,raw?&ops:NULL};}
static bool claim(const void* raw,const void* token)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;size_t references;
    if(!token||!count(raw,&references)||(self->claim&&self->claim!=token))return false;
    self->claim=token;return true;
}
/* Collector pins may be added after Claim. They are real counted references;
 * normal native/transport admission above remains quarantined by that claim. */
static bool pin(const void* raw)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;if(!self)return false;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    bool held=!self->cleared&&xrtRefRetain(&self->refs)>0;
    mutationEnd(&mutation);return held;
}
static void restore(const void* raw,const void* token)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;
    if(!token||self->claim!=token||self->cleared)abort();
    self->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;
    if(!token||self->claim!=token||self->cleared||busy(self))abort();
    self->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    XL6_TlsSessionV1* self=(XL6_TlsSessionV1*)raw;
    xrtownershipscope mutation={0};mutationBegin(&mutation);
    if(!token||self->claim!=token||!self->cleared||busy(self))abort();
    xtlssession* native=self->native;self->native=NULL;mutationEnd(&mutation);
    /* All native queued owners remain pinned; destroy outside graph Freeze.
     * This disposes storage, it does not fabricate authenticated TLS closure. */
    destroyNative(native);xl6TlsSelectorDropV1(&self->selector);xl6TlsResumeLookupDropV1(&self->resumeLookup);return true;
}
static const xrtownershipadapterv1 adapter={sizeof(adapter),pin,xl6TlsSessionDropInternalV1,claim,restore,NULL,clear,finish};
const xrtownershipadapterv1* xl6TlsSessionOwnershipAdapterV1(xrtownershipref ref)
{
    size_t references;return ref.Ops==&ops&&count(ref.Data,&references)?&adapter:NULL;
}
const xrtownershipquiescencev1* xl6TlsSessionQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&ops,busy};return &policy;}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* actual=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&actual,&user)&&raw&&!user&&
        actual==xl6TlsSessionHandlePolicyV1Get()->Ops&&visit(xl6TlsSessionOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsSessionHandlePolicyV1Get,xl6TlsSessionDestroyV1,handleTrace)
