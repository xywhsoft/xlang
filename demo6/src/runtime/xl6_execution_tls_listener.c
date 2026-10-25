#include "xl6_execution_tls_internal.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Listener {
    Resource base;Resource* parent;xatomicptr native;xatomic32 dataHeld;
    const XL6_TlsMetadataRoleV1* metadata;XL6_TlsSelectorV1 selector;
} Listener;
_Static_assert(offsetof(Listener,base)==0,"TLS resource base must be first");
static xtlslistener* native(const Resource* r)
{return xrtAtomicPtrLoad(&((const Listener*)r)->native,XMEMORY_ACQUIRE);}
static void dataDrop(const void* raw)
{
    Listener* r=(Listener*)raw;
    /* Last native owner has finished every borrowed Select invocation. Do not
     * run arbitrary capture destructors on this engine/drop thread: the real
     * source registration remains until its checked control tail cleans up. */
    xrtAtomic32Store(&r->dataHeld,0,XMEMORY_RELEASE);
    xl6ExecutionResourceNativeDropV1(raw);
}
const xtlslistenerownershipv1* xl6ExecutionTlsListenerDataPolicyV1Get(void)
{
    static const xtlslistenerownershipv1 policy={sizeof(policy),{0},{0},dataDrop,&xl6ExecutionResourceNativeOpsV1};
    return &policy;
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Listener* r=(Listener*)base;const XL6_TlsListenerCreateOptionsV1* c=options->value.tlsListener;
    xrtAtomicPtrInit(&r->native,NULL);
    xrtAtomic32Init(&r->dataHeld,0);
    if(!c||!c->config){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    r->metadata=c->metadata;
    /* Only the explicitly owning typed producer may provide Select. Native
     * config aliases and resumption callbacks are not admitted by inference. */
    if((c->selector.data&&(!c->selector.ops||!c->selector.ops->Ref||!c->selector.ops->Drop||
            !c->selector.ops->Ownership||!c->selector.ops->Select))||
        c->config->Tls.Select!=(c->selector.data?c->selector.ops->Select:NULL)||
        c->config->Tls.SelectContext!=c->selector.data||c->config->Tls.Resume||c->config->Tls.ResumeContext){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    r->selector=xl6TlsSelectorRefV1(c->selector);
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionTlsEngineEnterV1(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xtlslistener* result=NULL;
    if(r->parent&&xl6ExecutionResourceNativePinV1(base)){
        xrtAtomic32Store(&r->dataHeld,1,XMEMORY_RELEASE);
        result=xrtTlsListenerStartOwnedV1(scope.engine,c->config,base,xl6ExecutionTlsListenerDataPolicyV1Get());
        if(!result)dataDrop(base); /* Failed constructor consumed no credit. */
    }
    xrtAtomicPtrStore(&r->native,result,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);return result!=NULL;
}
static void detachNative(Resource* base)
{
    Listener* r=(Listener*)base;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    xtlslistener* listener=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtTlsListenerDestroy(listener);
}
static bool dispose(Resource* base)
{
    Listener* r=(Listener*)base;detachNative(base);
    xl6ExecutionTlsParentDropV1(&r->parent);xl6TlsSelectorDropV1(&r->selector);return true;
}
static bool rollback(Resource* base)
{return native(base)||xrtAtomic32Load(&((Listener*)base)->dataHeld,XMEMORY_ACQUIRE)?false:dispose(base);}
static XL6_ModulePrepareResult retire(Resource* base)
{
    xtlslistener* listener=native(base);
    if(listener){
        if(!xrtTlsListenerClose(listener))return XL6_MODULE_PREPARE_FAILED;
        if(xrtTlsListenerState(listener)!=XTLS_LISTENER_CLOSED)return XL6_MODULE_PREPARE_BUSY;
        detachNative(base);
    }
    /* CLOSED alone is insufficient: queued/pending records and a caller's
     * real native reference can still keep config borrows alive. Wait for the
     * aggregate Data credit, then clean captures inside the source's checked
     * retirement scope. No callbacks or arbitrary cleanup in dataDrop. */
    if(xrtAtomic32Load(&((Listener*)base)->dataHeld,XMEMORY_ACQUIRE))return XL6_MODULE_PREPARE_BUSY;
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
/* Graph Finish differs from ordinary Retire: the complete graph has already
 * certified quiescence and Cleared every node, so no borrowed selector can run
 * even when native/Data pins are released in the opposite Finish order. */
static void drop(Resource* base)
{const Listener* r=(const Listener*)base;if(native(base)||r->parent||r->selector.data||xrtAtomic32Load(&r->dataHeld,XMEMORY_ACQUIRE))abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Listener* r=(const Listener*)base;xtlslistener* listener=native(base);
    return (!listener||visit(xrtTlsListenerOwnership(listener),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context))&&
        (!r->selector.data||visit(r->selector.ops->Ownership(r->selector.data),context));
}
static bool closeListener(Resource* base){xtlslistener* l=native(base);return !l||xrtTlsListenerClose(l);}
static bool present(const Resource* base){return native(base)!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionTlsListenerFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_TLS_LISTENER_V1,
        create,rollback,retire,dispose,drop,trace,closeListener,closeListener,sizeof(Listener),present};return &family;
}
bool xl6ExecutionTlsListenerEnterV1(Resource* base,bool submit,XL6_TlsScopeV1* scope)
{
    if(!scope||scope->execution.resource){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ExecutionResourceEnterV1(base,submit,&scope->execution))return false;
    if(base->family!=xl6ExecutionTlsListenerFamilyV1Get()||base->kind!=XL6_EXECUTION_TLS_LISTENER_V1){
        xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_TYPE);return false;}
    scope->listener=native(base);scope->engineOwner=((Listener*)base)->parent;
    scope->metadata=((Listener*)base)->metadata;
    if(!scope->listener){scope->engineOwner=NULL;xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
