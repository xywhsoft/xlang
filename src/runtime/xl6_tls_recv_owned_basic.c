#include "xl6_tls_data_internal.h"
xfuture* xl6TlsStreamRecvOwnedAsyncV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    return xl6TlsFuturePublishV1(services,capture,source);
}
