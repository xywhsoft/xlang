#include "xl6_tcp_vector_internal.h"
int64_t xl6TcpStreamSendVectorBufferV1(XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* array,int64_t size)
{return xl6TcpVectorSendInternalV1(xl6NetVectorBuffersCaptureV1,context,receiver,array,size);}
