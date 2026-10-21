#include "xl6_execution_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_execution_net_engine.h"
#include "xl6_execution_net_resolver.h"
#include <stdlib.h>

typedef XL6_ExecutionResourceV1 Resource;
static bool enter(Resource* resource,const XL6_ExecutionFamilyV1* family,XL6_ExecutionScopeV1* scope)
{
    if(!xl6ExecutionResourceEnterV1(resource,true,scope))return false;
    if(resource->family!=family){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
Resource* xl6ExecutionTcpPrepareDialV1(XL6_ExecutionSourceV1* source,Resource* engine,Resource* resolver)
{
    XL6_ExecutionScopeV1 engineScope={0},resolverScope={0};Resource* result=NULL;
    if(!enter(engine,xl6ExecutionNetEngineFamilyV1Get(),&engineScope))return NULL;
    if(resolver&&!enter(resolver,xl6ExecutionNetResolverFamilyV1Get(),&resolverScope))goto done;
    XL6_TcpCreateOptionsV1 tcp={.engine=engine,.resolving=resolver,.dialing=true};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_STREAM_V1,.value.tcp=&tcp};
    result=xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpStreamFamilyV1Get(),&options);
done:
    if(resolverScope.resource)xl6ExecutionResourceLeaveV1(&resolverScope);
    xl6ExecutionResourceLeaveV1(&engineScope);return result;
}
static xfuture* start(Resource* resource,const char* host,uint16_t port,const xnetaddrlist* addresses,
    const xnetdialconfig* config,bool direct)
{
    if((direct&&!addresses)||(!direct&&(!host||!port))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionScopeV1 scope={0},engineScope={0},resolverScope={0};xfuture* result=NULL;
    if(!xl6ExecutionResourceControlV1(resource,true,&scope))return NULL;
    if(resource->family!=xl6ExecutionTcpStreamFamilyV1Get()){xrtSetErrorKind(XERR_TYPE);goto done;}
    XL6_TcpStreamResourceV1* carrier=(XL6_TcpStreamResourceV1*)resource;
    if(carrier->pendingKind!=XL6_TCP_PENDING_DIAL_V1||
        !xrtAtomic32Load(&carrier->prepared,XMEMORY_ACQUIRE)||
        xrtAtomicPtrLoad(&carrier->pending,XMEMORY_ACQUIRE)||
        (direct?(carrier->resolving!=NULL):(carrier->resolving==NULL))){
        xrtSetErrorKind(XERR_STATE);goto done;}
    if(!enter(carrier->parent,xl6ExecutionNetEngineFamilyV1Get(),&engineScope))goto done;
    if(!direct&&!enter(carrier->resolving,xl6ExecutionNetResolverFamilyV1Get(),&resolverScope))goto done;
    xfuture* pending=direct?xrtNetDialAddressesAsync(engineScope.engine,addresses,config,NULL,NULL):
        xrtNetDialAsync(engineScope.engine,resolverScope.resolver,host,port,config,NULL,NULL);
    if(pending){
        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
        xrtAtomicPtrStore(&carrier->pending,pending,XMEMORY_RELEASE);
        xrtAtomic32Store(&carrier->prepared,0,XMEMORY_RELEASE);
        if(!xrtOwnershipScopeEnd(&mutation))abort();
        result=xrtFutureRef(pending);
    }
done:
    if(resolverScope.resource)xl6ExecutionResourceLeaveV1(&resolverScope);
    if(engineScope.resource)xl6ExecutionResourceLeaveV1(&engineScope);
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
xfuture* xl6ExecutionTcpDialSourceV1(Resource* carrier,const char* host,uint16_t port,const xnetdialconfig* config)
{return start(carrier,host,port,NULL,config,false);}
xfuture* xl6ExecutionTcpDialAddressesSourceV1(Resource* carrier,const xnetaddrlist* addresses,const xnetdialconfig* config)
{return start(carrier,NULL,0,addresses,config,true);}
bool xl6ExecutionTcpPromoteDialV1(Resource* carrier)
{return xl6ExecutionTcpPromotePendingInternalV1(carrier,XL6_TCP_PENDING_DIAL_V1);}
