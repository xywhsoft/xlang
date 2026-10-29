#include "xl6_udp_message_internal.h"
int64_t xl6UdpSendMessageV1(void* receiver,const void* input)
{
    const XL6_UdpMessageV1* m=input;int result=XNET_RESULT_ERROR;size_t accepted=0;
    if(!m)xrtSetErrorKind(XERR_ARGUMENT);
    else result=xl6ExecutionUdpSendOwnedBatchV1(receiver,&m->native,1,&accepted);
    if(result==XNET_RESULT_ERROR)xl6UdpFailedV1();
    return result;
}
