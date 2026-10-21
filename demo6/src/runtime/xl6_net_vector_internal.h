#ifndef XL6_NET_VECTOR_INTERNAL_H
#define XL6_NET_VECTOR_INTERNAL_H
#include "xl6_net_vector_input.h"
#include "xl6_net_datagram_internal.h"
#include "xl6_net_socket_owner.h"
XL6_ProductI64U64V1 xl6NetVectorSendV1(XL6_NetVectorCaptureV1,XL6_NativeClassArrayContextV1,
    void*,const xvalue*,const void*,const void*,int,int64_t);
/* Four ordinary pointer+length calls, selected by declarative overloads. */
#define XL6_NET_VECTOR_PRIMITIVE_ENTRIES(Name,Capture) \
XL6_ProductI64U64V1 xl6NetSocketSendVector##Name##V1(void* r,const xvalue* v,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,NULL,NULL,0,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendVectorTo##Name##V1(void* r,const xvalue* v,const void* a,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,a,NULL,1,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendMessageVector##Name##V1(void* r,const xvalue* v,const void* a,const void* c,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,a,c,3,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageVector##Name##V1(void* r,const xvalue* v,const void* c,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,NULL,c,2,s);}
#endif
