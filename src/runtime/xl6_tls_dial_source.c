#include "xl6_tls_dial_internal.h"
xfuture* xl6TlsDialNativeSourceV1(XL6_ExecutionResourceV1* carrier,const XL6_TlsDialInputV1* input,const xnetproxy* proxy)
{
    if(proxy){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6ExecutionTlsDialSourceV1(carrier,input->host,input->port,&input->client,&input->config);
}
