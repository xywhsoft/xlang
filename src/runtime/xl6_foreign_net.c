#include "xl6_tls_role_context_internal.h"
#include "xl6_execution_resource.h"
#include "xl6_native_class_borrow.h"
#include "xl6_foreign_resource.h"
#include "include/xl_foreign_net.h"

static void* exact(const xvalue* value,const XL6_NativeHandlePolicyV1* expected)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!value||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=expected->Ops){
        xrtSetErrorKind(XERR_TYPE);return NULL;}
    return raw;
}
bool xl6ForeignNetClientVisitV1(const xvalue* engine,const xvalue* context,
    XL6_ForeignNetClientVisitV1 visit,void* data)
{
    if(!visit){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionResourceV1* resource=exact(engine,xl6NetEngineHandlePolicyV1Get());
    if(!resource)return false;
    const XL6_TlsClientContextV1* tls=NULL;
    if(context&&xrtValueType(context)!=XVALUE_NULL){
        tls=exact(context,xl6TlsClientContextHandlePolicyV1Get());if(!tls)return false;
    }
    xvalue* enginePin=xrtValueRetain((xvalue*)engine);
    xvalue* contextPin=context?xrtValueRetain((xvalue*)context):NULL;
    if(!enginePin||(context&&!contextPin)){xrtValueRelease(enginePin);xrtValueRelease(contextPin);return false;}
    XL6_ExecutionScopeV1 scope={0};bool entered=xl6ExecutionResourceEnterV1(resource,true,&scope);
    bool ok=false;
    if(entered){
        if(!scope.engine)xrtSetErrorKind(XERR_TYPE);
        else {
            xtlsclientconfig native;xrtTlsClientConfigInit(&native);
            if(tls){
                native.Context=tls->parameters;native.Verifier=tls->verifier.value;
                native.Resume=tls->resume.value;native.ResumeLimit=tls->ticketCapacity;
                native.ResumeOnly=tls->resumeRequired;
                if(tls->protocols){native.Protocols=tls->protocols->views;native.ProtocolCount=tls->protocols->count;}
            }
            ok=visit(scope.engine,tls?&native:NULL,data);
        }
    }
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,!ok))abort();
    if(entered)xl6ExecutionResourceLeaveV1(&scope);
    xrtValueRelease(contextPin);xrtValueRelease(enginePin);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return ok&&outcome.ok;
}

/* These typed entry points use the compiler-sealed field context, never a
 * guessed private key. Return the actual retained owning handle for tracing. */
xvalue* xl6ForeignNetEngineOwnerV1(XL6_NativeClassFieldContextV1 context,const xvalue* owner)
{
    void* raw=NULL;
    return xl6NativeClassFieldRetainV1(context,owner,xl6NetEngineHandlePolicyV1Get(),&raw);
}
xvalue* xl6ForeignTlsClientOwnerV1(XL6_NativeClassFieldContextV1 context,const xvalue* owner)
{
    void* raw=NULL;
    return xl6NativeClassFieldRetainV1(context,owner,xl6TlsClientContextHandlePolicyV1Get(),&raw);
}
