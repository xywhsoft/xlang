#include "xl6_execution_tls_dial_internal.h"
#include "xl6_execution_tls_dial.h"
static xfuture* submit(xnetengine* engine,xnetresolver* resolver,XL6_ExecutionResourceV1* data,
    const xtlsstreamownershipv1* policy,const XL6_TlsNativeDialArgumentsV1* a)
{return xrtTlsDialProxyAsyncOwnedV1(engine,resolver,a->proxy,a->host,a->port,a->client,a->config,data,policy);}
xfuture* xl6ExecutionTlsProxyDialSourceV1(XL6_ExecutionResourceV1* carrier,const xnetproxy* proxy,
    const char* host,uint16_t port,const xtlsclientconfig* client,const xtlsdialconfig* config)
{
    if(!proxy){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const XL6_TlsNativeDialArgumentsV1 args={host,port,proxy,client,config};
    return xl6ExecutionTlsDialSubmitV1(carrier,submit,&args);
}
