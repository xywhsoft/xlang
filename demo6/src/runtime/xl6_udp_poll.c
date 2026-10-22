#include "xl6_udp_internal.h"
void* xl6UdpTryReceiveV1(void* r)
{
    void* result=xl6ExecutionUdpReceiveV1(r);
    if(!result&&xrtGetError())xl6UdpFailedV1();
    return result;
}
void* xl6UdpTryReceiveErrorV1(void* r)
{
    void* result=xl6ExecutionUdpReceiveErrorV1(r);
    if(!result&&xrtGetError())xl6UdpFailedV1();
    return result;
}
