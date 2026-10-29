#include "xl6_tls_dial_internal.h"
#include "xl6_tls_dial_abi.h"
#include "xl6_future_callback_interrupt.h"
xfuture* xl6TlsDialAsyncCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,xstrview host,int64_t port,const void* role,const void* options,int64_t size,void* parent)
{
    if(!services.executor||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,false,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,XRT_DEADLINE_NEVER)){
        xrtFree(input.host);return xl6TlsFutureFailV1(capture,NULL);}
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1);
    xrtFree(input.host);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsDialAsyncUntilV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,xstrview host,int64_t port,const void* role,const void* options,int64_t size,uint64_t due)
{
    if(!services.executor||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,false,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due)){
        xrtFree(input.host);return xl6TlsFutureFailV1(capture,NULL);}
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1);
    xrtFree(input.host);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsDialAsyncUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,xstrview host,int64_t port,const void* role,const void* options,int64_t size,uint64_t due,void* parent)
{
    if(!services.executor||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,false,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due)){
        xrtFree(input.host);return xl6TlsFutureFailV1(capture,NULL);}
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1);
    xrtFree(input.host);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsDialAsyncTimeoutV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,xstrview host,int64_t port,const void* role,const void* options,int64_t size,int64_t millis)
{
    if(!services.executor||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,false,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    if(!xl6FutureCallbackInterruptArmV1(capture,NULL,services.deadline,due)){
        xrtFree(input.host);return xl6TlsFutureFailV1(capture,NULL);}
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1);
    xrtFree(input.host);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TlsDialAsyncTimeoutCancelV1(XL6_RuntimeServicesV1 services,XL6_ExecutionServicesV1 execution,XL6_NativeClassResultContextV1 context,void* engine,void* resolver,xstrview host,int64_t port,const void* role,const void* options,int64_t size,void* parent,int64_t millis)
{
    if(!services.executor||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){return xl6TlsFutureFailV1(NULL,NULL);}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,false,&input)){return xl6TlsFutureFailV1(NULL,NULL);}
    void* capture=xl6TlsClassFuturePrepareV1(context,NULL,NULL,xl6ExecutionTlsPromoteDialV1);
    if(!capture){xrtFree(input.host);return NULL;}
    if(!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due)){
        xrtFree(input.host);return xl6TlsFutureFailV1(capture,NULL);}
    bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6TlsDialClassSourceV1(execution,capture,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1);
    xrtFree(input.host);
    if(!source&&!stopped)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
