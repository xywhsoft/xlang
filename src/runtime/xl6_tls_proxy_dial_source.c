#include "xl6_tls_dial_internal.h"
xfuture* xl6TlsProxyDialNativeSourceV1(XL6_ExecutionResourceV1* carrier,const XL6_TlsDialInputV1* input,const xnetproxy* proxy)
{return xl6ExecutionTlsProxyDialSourceV1(carrier,proxy,input->host,input->port,&input->client,&input->config);}
