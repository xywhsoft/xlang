#include "xl6_udp_vector_internal.h"
int64_t xl6UdpSendVectorBufferV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* a,int64_t s)
{return xl6UdpVectorSendInternalV1(xl6NetVectorBuffersCaptureV1,c,r,NULL,NULL,a,s);}
int64_t xl6UdpSendVectorToBufferV1(XL6_NativeClassArrayContextV1 c,void* r,const void* target,const xvalue* a,int64_t s)
{return xl6UdpVectorSendInternalV1(xl6NetVectorBuffersCaptureV1,c,r,target,NULL,a,s);}
int64_t xl6UdpSendVectorMessageBufferV1(XL6_NativeClassArrayContextV1 c,void* r,const void* target,const void* control,const xvalue* a,int64_t s)
{return xl6UdpVectorSendInternalV1(xl6NetVectorBuffersCaptureV1,c,r,target,control,a,s);}
int64_t xl6UdpSendVectorConnectedMessageBufferV1(XL6_NativeClassArrayContextV1 c,void* r,const void* control,const xvalue* a,int64_t s)
{return xl6UdpVectorSendInternalV1(xl6NetVectorBuffersCaptureV1,c,r,NULL,control,a,s);}
