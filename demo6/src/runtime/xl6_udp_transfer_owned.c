#include "xl6_udp_transfer_internal.h"
#include "xl6_udp_message_internal.h"
static bool fill(void* out,void* raw){*(xnetudpownedmessage*)out=((const XL6_UdpMessageV1*)raw)->native;return true;}
XL6_ProductI64U64V1 xl6UdpSendMessagesV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* a,void* p,int64_t start,int64_t count)
{return xl6UdpBatchSendInternalV1(c,r,a,p,start,count,true,xl6UdpMessageHandlePolicyV1Get(),fill);}
