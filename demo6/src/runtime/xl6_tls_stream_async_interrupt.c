#include "xl6_tls_io_internal.h"
#include "xl6_future_callback_interrupt.h"
xfuture* xl6TlsStreamRecvAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamRecvAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamWaitAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamWaitAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamWaitAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamWaitAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamWaitAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendTextAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendTextAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendTextAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendTextAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendTextAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,xstrview input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsTextInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBytesAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBytesAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBytesAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBytesAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBytesAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBytesInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBufferAsyncCancelV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBufferAsyncUntilV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size,uint64_t due)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBufferAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBufferAsyncTimeoutV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsStreamSendBufferAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,void* receiver,void* input,int64_t size,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xbytesview data;if(!xl6TlsBufferInputV1(input,size,&data)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsIoFuturePrepareV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!capture)return NULL;
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due))return xl6TlsFutureFailV1(capture,NULL);
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsIoSourceV1(XL6_TLS_IO_SEND_V1,receiver,0,data);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
