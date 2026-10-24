#include "xl6_tls_io_internal.h"
#include "xl6_future_callback_interrupt.h"
xfuture* xl6TlsListenerAcceptAsyncCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsAcceptSourceV1(execution,receiver,capture);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsListenerAcceptAsyncUntilV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsAcceptSourceV1(execution,receiver,capture);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsListenerAcceptAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsAcceptSourceV1(execution,receiver,capture);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsListenerAcceptAsyncTimeoutV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsAcceptSourceV1(execution,receiver,capture);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsListenerAcceptAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* receiver,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsAcceptFuturePrepareV1(context,receiver);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsAcceptSourceV1(execution,receiver,capture);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
