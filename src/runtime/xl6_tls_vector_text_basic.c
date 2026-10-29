#include "xl6_tls_data_internal.h"
static bool acquire(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array)
{(void)context;return xl6NetVectorPrimitiveCaptureV1(input,array,TEXT_INPUT,xl6StringArrayAdmissionV1);}
xfuture* xl6TlsStreamSendVectorTextAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    size_t count=0;
    xfuture* source=xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    return xl6TlsFuturePublishV1(services,capture,source);
}
