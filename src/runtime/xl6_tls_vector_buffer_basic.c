#include "xl6_tls_data_internal.h"
xfuture* xl6TlsStreamSendVectorBufferAsyncV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    size_t count=0;
    xfuture* source=xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    return xl6TlsFuturePublishV1(services,capture,source);
}
