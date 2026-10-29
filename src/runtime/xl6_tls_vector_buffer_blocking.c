#include "xl6_tls_data_internal.h"
int64_t xl6TlsStreamSendVectorBufferV1(XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,due,NULL);
}
int64_t xl6TlsStreamSendVectorBufferCancelV1(XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,due,parent);
}
int64_t xl6TlsStreamSendVectorBufferUntilV1(XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    return xl6TlsVectorSyncV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,due,NULL);
}
int64_t xl6TlsStreamSendVectorBufferUntilCancelV1(XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(xl6NetVectorBuffersCaptureV1,context,receiver,input,size,due,parent);
}
