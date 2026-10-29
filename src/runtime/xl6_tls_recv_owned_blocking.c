#include "xl6_tls_data_internal.h"
void* xl6TlsStreamRecvOwnedV1(void* receiver,int64_t maximum,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvOwnedSyncV1(receiver,maximum,due,NULL);
}
void* xl6TlsStreamRecvOwnedCancelV1(void* receiver,int64_t maximum,void* parent,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return NULL;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvOwnedSyncV1(receiver,maximum,due,parent);
}
void* xl6TlsStreamRecvOwnedUntilV1(void* receiver,int64_t maximum,uint64_t due)
{
    return xl6TlsRecvOwnedSyncV1(receiver,maximum,due,NULL);
}
void* xl6TlsStreamRecvOwnedUntilCancelV1(void* receiver,int64_t maximum,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    return xl6TlsRecvOwnedSyncV1(receiver,maximum,due,parent);
}
