#include "xl6_udp_vector_internal.h"
static bool acquire(XL6_NetVectorInputsV1* i,XL6_NativeClassArrayContextV1 c,const xvalue* a)
{(void)c;return xl6NetVectorPrimitiveCaptureV1(i,a,TEXT_INPUT,xl6StringArrayAdmissionV1);}
XL6_UDP_VECTOR_ENTRIES(Text,acquire)
