#include "xl6_tls_data_internal.h"
#include "xl6_future_callback_interrupt.h"
xfuture* xl6TlsStreamSendVectorBufferAsyncCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XL6_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBufferAsyncUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBufferAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBufferAsyncTimeoutV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendVectorBufferAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    size_t count=0;
    xfuture* source=stopped?NULL:xl6TlsVectorSourceV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,&count);
    if(source&&!xl6TlsIoFutureSetSendSizeV1(capture,count))return xl6TlsFutureFailV1(capture,source);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
