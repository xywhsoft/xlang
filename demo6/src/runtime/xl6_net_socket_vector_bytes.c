#include "xl6_net_vector_internal.h"
static bool acquire(XL6_NetVectorInputsV1* input,XL6_NativeClassArrayContextV1 context,const xvalue* array)
{(void)context;return xl6NetVectorPrimitiveCaptureV1(input,array,BYTES_INPUT,xl6ReferenceBytesAdmissionV1);}
XL6_NET_VECTOR_PRIMITIVE_ENTRIES(Bytes,acquire)
