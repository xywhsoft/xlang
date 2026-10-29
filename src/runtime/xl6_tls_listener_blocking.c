#include "xl6_tls_io_internal.h"
void* xl6TlsListenerAcceptV1(XL6_ExecutionServicesV1 execution,void* receiver,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsAcceptSyncV1(execution,receiver,due,NULL);
}
void* xl6TlsListenerAcceptCancelV1(XL6_ExecutionServicesV1 execution,void* receiver,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsAcceptSyncV1(execution,receiver,due,parent);
}
void* xl6TlsListenerAcceptUntilV1(XL6_ExecutionServicesV1 execution,void* receiver,uint64_t due)
{
    return xl6TlsAcceptSyncV1(execution,receiver,due,NULL);
}
void* xl6TlsListenerAcceptUntilCancelV1(XL6_ExecutionServicesV1 execution,void* receiver,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsAcceptSyncV1(execution,receiver,due,parent);
}
