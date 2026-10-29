#include "xl6_tls_dial_internal.h"
xfuture* xl6TlsDialClassSourceV1(XL6_ExecutionServicesV1 services,void* raw,void* engine,void* resolver,
    const XL6_TlsDialInputV1* input,const xnetproxy* proxy,XL6_TlsDialSourceV1 submit)
{
    if(!raw||!submit||!input){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTlsPrepareDialV1(services.source,engine,resolver);
    if(!carrier)return NULL;
    XL6_ExecutionResourceV1* borrowed=carrier;
    if(!xl6TlsClassFutureBindV1(raw,&carrier)){xl6ExecutionResourceReleaseV1(carrier);return NULL;}
    return submit(borrowed,input,proxy);
}
