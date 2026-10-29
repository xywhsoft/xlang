#include "xl6_udp_internal.h"
#include "xl6_net_datagram_internal.h"
#include "xl6_net_datagram_error_internal.h"
void* xl6UdpPacketRemoteV1(const void* raw)
{
    void* result=xl6NetAddressCopyV1(xrtNetUdpPacketRemote(raw));if(!result)xl6UdpFailedV1();return result;
}
void* xl6UdpPacketInfoV1(const void* raw)
{
    const xnetaddr* remote=xrtNetUdpPacketRemote(raw);const xnetdgrammeta* meta=xrtNetUdpPacketMeta(raw);
    if(!remote||!meta){xrtSetErrorKind(XERR_STATE);xl6UdpFailedV1();return NULL;}
    XL6_NetDatagramInfoV1* result=xrtMalloc(sizeof(*result));
    if(result)*result=(XL6_NetDatagramInfoV1){*remote,*meta,true};else xl6UdpFailedV1();return result;
}
void* xl6UdpErrorPacketInfoV1(const void* raw)
{
    const xnetdgramerror* value=xrtNetUdpErrorPacketInfo(raw);
    if(!value){xrtSetErrorKind(XERR_STATE);xl6UdpFailedV1();return NULL;}
    XL6_NetDatagramErrorInfoV1* result=xrtMalloc(sizeof(*result));
    if(result)*result=(XL6_NetDatagramErrorInfoV1){*value,true};else xl6UdpFailedV1();return result;
}
