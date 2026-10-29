#include "xl6_tls_data_internal.h"
#include "xl6_future_callback_interrupt.h"
xfuture* xl6TlsStreamRecvOwnedAsyncCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XL6_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvOwnedAsyncUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvOwnedAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvOwnedAsyncTimeoutV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvOwnedAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum,void* parent,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsRecvOwnedFuturePrepareV1(context,receiver,maximum);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
