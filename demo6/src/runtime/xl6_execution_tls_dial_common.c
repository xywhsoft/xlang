#include "xl6_execution_tls_dial_internal.h"
#include "xl6_execution_tls_dial.h"
#include "xl6_execution_net_resolver.h"
typedef XL6_ExecutionResourceV1 Resource;
static bool resolverEnter(Resource* raw,XL6_ExecutionScopeV1* scope)
{
    if(!xl6ExecutionResourceEnterV1(raw,true,scope))return false;
    if(raw->family!=xl6ExecutionNetResolverFamilyV1Get()||!scope->resolver){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
Resource* xl6ExecutionTlsPrepareDialV1(XL6_ExecutionSourceV1* source,Resource* engine,Resource* resolver)
{
    XL6_ExecutionScopeV1 network={0},resolving={0};Resource* result=NULL;
    if(!xl6ExecutionTlsEngineEnterV1(engine,&network))return NULL;
    if(resolverEnter(resolver,&resolving)){
        const XL6_TlsStreamCreateOptionsV1 tls={.engine=engine,.resolving=resolver,.dialing=true,.metadata=xl6TlsClientMetadataRoleV1Get()};
        const XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TLS_STREAM_V1,.value.tlsStream=&tls};
        result=xl6ExecutionCreateInternalV1(source,xl6ExecutionTlsStreamFamilyV1Get(),&options);
        xl6ExecutionResourceLeaveV1(&resolving);
    }
    xl6ExecutionResourceLeaveV1(&network);return result;
}
xfuture* xl6ExecutionTlsDialSubmitV1(Resource* base,XL6_TlsNativeDialSubmitV1 submit,const XL6_TlsNativeDialArgumentsV1* args)
{
    if(!submit||!args||!args->host||!args->port||!args->client){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionScopeV1 scope={0},engine={0},resolver={0};xfuture* result=NULL;
    if(!xl6ExecutionResourceControlV1(base,true,&scope))return NULL;
    if(base->family!=xl6ExecutionTlsStreamFamilyV1Get()){xrtSetErrorKind(XERR_TYPE);goto done;}
    XL6_TlsStreamResourceV1* r=(XL6_TlsStreamResourceV1*)base;
    if(r->pendingKind!=XL6_TLS_PENDING_DIAL_V1||!r->resolving||
        !xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)||xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE)){
        xrtSetErrorKind(XERR_STATE);goto done;}
    if(!xl6ExecutionTlsEngineEnterV1(r->parent,&engine)||!resolverEnter(r->resolving,&resolver))goto done;
    if(xl6ExecutionResourceNativePinV1(base)){
        xfuture* pending=submit(engine.engine,resolver.resolver,base,xl6ExecutionTlsStreamDataPolicyV1Get(),args);
        if(!pending)xl6ExecutionResourceNativeDropV1(base);
        else{
            xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
            xrtAtomicPtrStore(&r->pending,pending,XMEMORY_RELEASE);xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
            if(!xrtOwnershipScopeEnd(&mutation))abort();
            result=xrtFutureRef(pending);
        }
    }
done:
    if(resolver.resource)xl6ExecutionResourceLeaveV1(&resolver);
    if(engine.resource)xl6ExecutionResourceLeaveV1(&engine);
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
bool xl6ExecutionTlsPromoteDialV1(Resource* base)
{return xl6ExecutionTlsPromotePendingV1(base,XL6_TLS_PENDING_DIAL_V1);}
