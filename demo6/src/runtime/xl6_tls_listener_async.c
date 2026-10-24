#include "xl6_tls_io_internal.h"
xfuture* xl6TlsListenerAcceptAsyncV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    xfuture* source=xl6TlsAcceptSourceV1(execution,receiver,capture);
    return xl6TlsFuturePublishV1(services,capture,source);
}
