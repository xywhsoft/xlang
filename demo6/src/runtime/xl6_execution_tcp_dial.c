#include "xl6_execution_tcp_dial_internal.h"
typedef struct XL6_HostDialArguments {
    const char* host; uint16_t port; const xnetdialconfig* config;
} XL6_HostDialArguments;
typedef struct XL6_AddressDialArguments {
    const xnetaddrlist* addresses; const xnetdialconfig* config;
} XL6_AddressDialArguments;
static xfuture* hostSubmit(xnetengine* engine,xnetresolver* resolver,const void* data)
{
    const XL6_HostDialArguments* a=data;
    return xrtNetDialAsync(engine,resolver,a->host,a->port,a->config,NULL,NULL);
}
static xfuture* addressesSubmit(xnetengine* engine,xnetresolver* resolver,const void* data)
{
    (void)resolver;const XL6_AddressDialArguments* a=data;
    return xrtNetDialAddressesAsync(engine,a->addresses,a->config,NULL,NULL);
}
xfuture* xl6ExecutionTcpDialSourceV1(XL6_ExecutionResourceV1* carrier,const char* host,
    uint16_t port,const xnetdialconfig* config)
{
    if(!host||!port){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_HostDialArguments args={host,port,config};
    return xl6ExecutionTcpDialSubmitInternalV1(carrier,true,hostSubmit,&args);
}
xfuture* xl6ExecutionTcpDialAddressesSourceV1(XL6_ExecutionResourceV1* carrier,
    const xnetaddrlist* addresses,const xnetdialconfig* config)
{
    if(!addresses){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_AddressDialArguments args={addresses,config};
    return xl6ExecutionTcpDialSubmitInternalV1(carrier,false,addressesSubmit,&args);
}
