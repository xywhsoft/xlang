#include "xl6_tcp_vector_internal.h"
static bool acquire(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array)
{(void)context;return xl6NetVectorPrimitiveCaptureV1(input,array,BYTES_INPUT,xl6ReferenceBytesAdmissionV1);}
int64_t xl6TcpStreamSendVectorBytesV1(void* receiver,const xvalue* array,int64_t size)
{return xl6TcpVectorSendInternalV1(acquire,(XL6_NativeClassArrayContextV1){0},receiver,array,size);}
