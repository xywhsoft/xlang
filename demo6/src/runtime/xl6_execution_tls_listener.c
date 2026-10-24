#include "xl6_execution_tls_internal.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Listener {Resource base;Resource* parent;xatomicptr native;} Listener;
_Static_assert(offsetof(Listener,base)==0,"TLS resource base must be first");
static xtlslistener* native(const Resource* r)
{return xrtAtomicPtrLoad(&((const Listener*)r)->native,XMEMORY_ACQUIRE);}
const xtlslistenerownershipv1* xl6ExecutionTlsListenerDataPolicyV1Get(void)
{
    static const xtlslistenerownershipv1 policy={sizeof(policy),{0},{0},xl6ExecutionResourceNativeDropV1,&xl6ExecutionResourceNativeOpsV1};
    return &policy;
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Listener* r=(Listener*)base;const XL6_TlsListenerCreateOptionsV1* c=options->value.tlsListener;
    xrtAtomicPtrInit(&r->native,NULL);
    if(!c||!c->config){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    /* This provider certifies static role contexts only. Dynamic selectors
     * need their own owned capture/trace integration, never borrowed aliases. */
    if(c->config->Tls.Select||c->config->Tls.SelectContext||c->config->Tls.Resume||c->config->Tls.ResumeContext){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionTlsEngineEnterV1(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xtlslistener* result=NULL;
    if(r->parent&&xl6ExecutionResourceNativePinV1(base)){
        result=xrtTlsListenerStartOwnedV1(scope.engine,c->config,base,xl6ExecutionTlsListenerDataPolicyV1Get());
        if(!result)xl6ExecutionResourceNativeDropV1(base);
    }
    xrtAtomicPtrStore(&r->native,result,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);return result!=NULL;
}
static bool dispose(Resource* base)
{
    Listener* r=(Listener*)base;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    xtlslistener* listener=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtTlsListenerDestroy(listener);xl6ExecutionTlsParentDropV1(&r->parent);return true;
}
static bool rollback(Resource* base){return native(base)?false:dispose(base);}
static XL6_ModulePrepareResult retire(Resource* base)
{
    xtlslistener* listener=native(base);if(!listener)return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    if(!xrtTlsListenerClose(listener))return XL6_MODULE_PREPARE_FAILED;
    if(xrtTlsListenerState(listener)!=XTLS_LISTENER_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static void drop(Resource* base){if(native(base)||((Listener*)base)->parent)abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Listener* r=(const Listener*)base;xtlslistener* listener=native(base);
    return (!listener||visit(xrtTlsListenerOwnership(listener),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
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
    if(!scope->listener){scope->engineOwner=NULL;xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
