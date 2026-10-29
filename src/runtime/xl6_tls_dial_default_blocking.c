#include "xl6_tls_dial_internal.h"
#include "xl6_tls_dial_abi.h"
void* xl6TlsDialDefaultV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,xstrview host,int64_t port,const void* role,int64_t size,int64_t millis)
{
    if(!execution.source){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,NULL,false,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1,due,NULL);
    xrtFree(input.host);return result;
}
void* xl6TlsDialCancelDefaultV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,xstrview host,int64_t port,const void* role,int64_t size,void* parent,int64_t millis)
{
    if(!execution.source){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,NULL,false,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1,due,parent);
    xrtFree(input.host);return result;
}
void* xl6TlsDialUntilDefaultV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,xstrview host,int64_t port,const void* role,int64_t size,uint64_t due)
{
    if(!execution.source){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,NULL,false,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1,due,NULL);
    xrtFree(input.host);return result;
}
void* xl6TlsDialUntilCancelDefaultV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,xstrview host,int64_t port,const void* role,int64_t size,uint64_t due,void* parent)
{
    if(!execution.source){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,NULL,false,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,NULL,xl6TlsDialNativeSourceV1,due,parent);
    xrtFree(input.host);return result;
}
