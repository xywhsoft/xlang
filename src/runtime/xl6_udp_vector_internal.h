#ifndef XL6_UDP_VECTOR_INTERNAL_H
#define XL6_UDP_VECTOR_INTERNAL_H
#include "xl6_udp_internal.h"
#include "xl6_net_vector_input.h"
int64_t xl6UdpVectorSendInternalV1(XL6_NetVectorCaptureV1,XL6_NativeClassArrayContextV1,void*,const void*,const void*,const xvalue*,int64_t);
#define XL6_UDP_VECTOR_ENTRIES(Name,Capture) \
int64_t xl6UdpSendVector##Name##V1(void* r,const xvalue* v,int64_t s){return xl6UdpVectorSendInternalV1(Capture,(XL6_NativeClassArrayContextV1){0},r,NULL,NULL,v,s);} \
int64_t xl6UdpSendVectorTo##Name##V1(void* r,const void* a,const xvalue* v,int64_t s){return xl6UdpVectorSendInternalV1(Capture,(XL6_NativeClassArrayContextV1){0},r,a,NULL,v,s);} \
int64_t xl6UdpSendVectorMessage##Name##V1(void* r,const void* a,const void* c,const xvalue* v,int64_t s){return xl6UdpVectorSendInternalV1(Capture,(XL6_NativeClassArrayContextV1){0},r,a,c,v,s);} \
int64_t xl6UdpSendVectorConnectedMessage##Name##V1(void* r,const void* c,const xvalue* v,int64_t s){return xl6UdpVectorSendInternalV1(Capture,(XL6_NativeClassArrayContextV1){0},r,NULL,c,v,s);}
#endif
