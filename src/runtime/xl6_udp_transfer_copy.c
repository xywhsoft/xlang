#include "xl6_udp_transfer_internal.h"
#include "xl6_net_datagram_batch_internal.h"
static bool fill(void* out,void* raw)
{
    XL6_NetDatagramPacketV1* packet=raw;xbuffer* bytes=xl6NetDatagramPacketBufferV1(packet);
    if(!bytes)return false;
    *(xnetdgramsend*)out=(xnetdgramsend){packet->hasTarget?&packet->target:NULL,bytes->Data,bytes->Size};return true;
}
XL6_ProductI64U64V1 xl6UdpSendPacketBatchV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* a,void* p,int64_t start,int64_t count)
{return xl6UdpBatchSendInternalV1(c,r,a,p,start,count,false,xl6NetDatagramPacketHandlePolicyV1Get(),fill);}
