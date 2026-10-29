#include "xl6_udp_internal.h"
static void* address(void* raw,bool peer)
{
    xnetaddr value;bool ok=peer?xl6ExecutionUdpPeerV1(raw,&value):xl6ExecutionUdpLocalV1(raw,&value);
    if(!ok){xl6UdpFailedV1();return NULL;}
    void* result=xl6NetAddressCopyV1(&value);if(!result)xl6UdpFailedV1();return result;
}
void* xl6UdpLocalV1(void* raw){return address(raw,false);}
void* xl6UdpPeerV1(void* raw){return address(raw,true);}
