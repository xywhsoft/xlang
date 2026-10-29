#include "xl6_execution_tcp_dial_internal.h"
typedef struct XL6_ProxyDialArguments {
    const xnetproxy* proxy; const char* host; uint16_t port;
    const xnetproxydialconfig* config;
} XL6_ProxyDialArguments;
static xfuture* proxySubmit(xnetengine* engine,xnetresolver* resolver,const void* data)
{
    const XL6_ProxyDialArguments* a=data;
    return xrtNetProxyDialAsync(engine,resolver,a->proxy,a->host,a->port,a->config,NULL,NULL);
}
xfuture* xl6ExecutionTcpProxyDialSourceV1(XL6_ExecutionResourceV1* carrier,
    const xnetproxy* proxy,const char* host,uint16_t port,const xnetproxydialconfig* config)
{
    if(!proxy||!host||!port){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ProxyDialArguments args={proxy,host,port,config};
    return xl6ExecutionTcpDialSubmitInternalV1(carrier,true,proxySubmit,&args);
}
