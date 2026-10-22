#include "xl6_udp_internal.h"
static int64_t sendRef(void* receiver,const void* remote,const void* control,void* input,int64_t size,int64_t offset,bool packet)
{
    xbytesview view=packet?(xbytesview){xrtNetUdpPacketData(input),xrtNetUdpPacketSize(input)}:xrtNetBytesView(input);
    if(!xl6UdpSliceV1(&view,size,offset)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}
    int result=packet?xl6ExecutionUdpSendPacketV1(receiver,remote,control,input,(size_t)offset,view.Size):
        xl6ExecutionUdpSendBytesV1(receiver,remote,control,input,(size_t)offset,view.Size);
    if(result==XNET_RESULT_ERROR)xl6UdpFailedV1();
    return result;
}
#define REF(Name,Packet) \
int64_t xl6UdpSendRef##Name##V1(void* r,void* value,int64_t size,int64_t offset){return sendRef(r,NULL,NULL,value,size,offset,Packet);} \
int64_t xl6UdpSendRefTo##Name##V1(void* r,const void* a,void* value,int64_t size,int64_t offset){return sendRef(r,a,NULL,value,size,offset,Packet);} \
int64_t xl6UdpSendRefMessage##Name##V1(void* r,const void* a,const void* c,void* value,int64_t size,int64_t offset){return sendRef(r,a,c,value,size,offset,Packet);} \
int64_t xl6UdpSendRefConnectedMessage##Name##V1(void* r,const void* c,void* value,int64_t size,int64_t offset){return sendRef(r,NULL,c,value,size,offset,Packet);}
REF(Bytes,false)
REF(Packet,true)
#undef REF
