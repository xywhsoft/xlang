#include "xl6_execution_tls_dial_internal.h"
#include "xl6_execution_tls_dial.h"
static xfuture* submit(xnetengine* engine,xnetresolver* resolver,XL6_ExecutionResourceV1* data,
    const xtlsstreamownershipv1* policy,const XL6_TlsNativeDialArgumentsV1* a)
{return xrtTlsDialAsyncOwnedV1(engine,resolver,a->host,a->port,a->client,a->config,data,policy);}
xfuture* xl6ExecutionTlsDialSourceV1(XL6_ExecutionResourceV1* carrier,const char* host,uint16_t port,
    const xtlsclientconfig* client,const xtlsdialconfig* config)
{
    const XL6_TlsNativeDialArgumentsV1 args={.host=host,.port=port,.client=client,.config=config};
    return xl6ExecutionTlsDialSubmitV1(carrier,submit,&args);
}
