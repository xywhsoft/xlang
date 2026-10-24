#include "xl6_tls_dial_internal.h"
#include "xl6_tls_dial_abi.h"
xfuture* xl6TlsProxyDialAsyncDefaultV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* role,int64_t size)
{
    if(!services.executor||!execution.source||!proxy){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,NULL,true,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    xfuture* source=xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,proxy,xl6TlsProxyDialNativeSourceV1);
    xrtFree(input.host);
    return xl6TlsFuturePublishV1(services,capture,source);
}
