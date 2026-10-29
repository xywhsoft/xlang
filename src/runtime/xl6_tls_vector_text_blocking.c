#include "xl6_tls_data_internal.h"
static bool acquire(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array)
{(void)context;return xl6NetVectorPrimitiveCaptureV1(input,array,TEXT_INPUT,xl6StringArrayAdmissionV1);}
int64_t xl6TlsStreamSendVectorTextV1(void* receiver,const xvalue* input,int64_t size,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,due,NULL);
}
int64_t xl6TlsStreamSendVectorTextCancelV1(void* receiver,const xvalue* input,int64_t size,void* parent,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TlsTransportFailedV1();return 0;}
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,due,parent);
}
int64_t xl6TlsStreamSendVectorTextUntilV1(void* receiver,const xvalue* input,int64_t size,uint64_t due)
{
    return xl6TlsVectorSyncV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,due,NULL);
}
int64_t xl6TlsStreamSendVectorTextUntilCancelV1(void* receiver,const xvalue* input,int64_t size,uint64_t due,void* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return 0;}
    return xl6TlsVectorSyncV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,input,size,due,parent);
}
