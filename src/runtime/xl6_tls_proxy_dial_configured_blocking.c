#include "xl6_tls_dial_internal.h"
#include "xl6_tls_dial_abi.h"
void* xl6TlsProxyDialV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* role,const void* options,int64_t size,int64_t millis)
{
    if(!execution.source||!proxy){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,true,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,proxy,xl6TlsProxyDialNativeSourceV1,due,NULL);
    xrtFree(input.host);return result;
}
void* xl6TlsProxyDialCancelV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* role,const void* options,int64_t size,void* parent,int64_t millis)
{
    if(!execution.source||!proxy){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,true,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,proxy,xl6TlsProxyDialNativeSourceV1,due,parent);
    xrtFree(input.host);return result;
}
void* xl6TlsProxyDialUntilV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* role,const void* options,int64_t size,uint64_t due)
{
    if(!execution.source||!proxy){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,true,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,proxy,xl6TlsProxyDialNativeSourceV1,due,NULL);
    xrtFree(input.host);return result;
}
void* xl6TlsProxyDialUntilCancelV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* role,const void* options,int64_t size,uint64_t due,void* parent)
{
    if(!execution.source||!proxy){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsDialInputV1 input={0};if(!xl6TlsDialInputReadV1(host,size,port,role,options,true,&input)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6TlsDialSyncV1(execution,engine,resolver,&input,proxy,xl6TlsProxyDialNativeSourceV1,due,parent);
    xrtFree(input.host);return result;
}
