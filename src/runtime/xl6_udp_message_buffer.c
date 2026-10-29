#include "xl6_udp_message_internal.h"
typedef XL6_UdpMessageV1 Message;
void* xl6UdpMessageBufferV1(void* input,int64_t size)
{xbytesview view={0};Message* m=xl6BufferInput(input,size,&view)?xl6UdpMessageCopyInternalV1(view,-1):NULL;if(!m)xl6UdpFailedV1();return m;}
