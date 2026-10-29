#include "xl6_tls_data_internal.h"
#include "xl6_future_callback_interrupt.h"
static bool acquire(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array)
{(void)context;return xl6NetVectorPrimitiveCaptureV1(input,array,BYTES_INPUT,xl6ReferenceBytesAdmissionV1);}
xfuture* xl6TlsStreamSendVectorBytesAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBytesAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBytesAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBytesAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBytesAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
