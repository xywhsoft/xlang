#include "xl6_tls_io_internal.h"
xfuture* xl6TlsStreamRecvAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    return xl6TlsFuturePublishV1(services,capture,source);
}
xfuture* xl6TlsStreamWaitAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    return xl6TlsFuturePublishV1(services,capture,source);
}
xfuture* xl6TlsStreamSendTextAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    return xl6TlsFuturePublishV1(services,capture,source);
}
xfuture* xl6TlsStreamSendBytesAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    return xl6TlsFuturePublishV1(services,capture,source);
}
xfuture* xl6TlsStreamSendBufferAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    xfuture* source=xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    return xl6TlsFuturePublishV1(services,capture,source);
}
