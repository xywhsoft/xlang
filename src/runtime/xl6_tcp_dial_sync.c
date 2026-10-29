#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_execution_net_engine.h"
#include "xl6_net_resolver_input.h"
#include "xl6_future_payload.h"
#include "xl6_tcp_dial_sync_internal.h"
void* xl6TcpDialV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,
    xstrview input,int64_t port,const void* options)
{
    if(!resolver||port<=0||port>UINT16_MAX){xrtSetErrorKind(!resolver?XERR_ARGUMENT:XERR_RANGE);xl6TcpFailedV1();return NULL;}
    const xnetdialconfig* config=options;
    char* host=xl6NetResolverHostV1(input,config?config->Family:XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    if(!xl6TcpDialMayWaitInternalV1(engine)){xrtFree(host);xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,resolver);
    xfuture* source=carrier?xl6ExecutionTcpDialSourceV1(carrier,host,(uint16_t)port,config):NULL;
    xrtFree(host);return xl6TcpDialFinishUntilInternalV1(carrier,source,XL6_DEADLINE_NEVER,NULL);
}
void* xl6TcpDialDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port)
{return xl6TcpDialV1(services,engine,resolver,host,port,NULL);}
void* xl6TcpDialAddressesV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,const void* options)
{
    if(!addresses){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(!xl6TcpDialMayWaitInternalV1(engine)){xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,NULL);
    xfuture* source=carrier?xl6ExecutionTcpDialAddressesSourceV1(carrier,addresses,options):NULL;
    return xl6TcpDialFinishUntilInternalV1(carrier,source,XL6_DEADLINE_NEVER,NULL);
}
void* xl6TcpDialAddressesDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses)
{return xl6TcpDialAddressesV1(services,engine,addresses,NULL);}
