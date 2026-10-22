#include "udp_language_native.c"
xnetudpbatch* udpFixtureBatch(void)
{
    xnetaddr address;if(!xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0))return NULL;
    xnetudpbatch* batch=__xrtNetUdpBatchCreate(2);if(!batch)return NULL;
    xnetudppacket* packet=udpFixturePacket(&address);if(!packet){xrtNetUdpBatchDestroy(batch);return NULL;}
    xrtAtomicPtrStore(&batch->Packets[0],packet,XMEMORY_RELEASE);batch->Count=1;return batch;
}
