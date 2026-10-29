#include "xl6_udp_internal.h"
/* The handle owns the actual immutable native result, not a copied payload,
 * fake creator or receiver-dependent pointer. No public point constructor. */
void xl6UdpPacketDestroyV1(void* raw){xrtNetUdpPacketDestroy(raw);}
uint64_t xl6UdpPacketLengthV1(const void* raw){return xrtNetUdpPacketSize(raw);}
bool xl6UdpPacketTruncatedV1(const void* raw){return xrtNetUdpPacketTruncated(raw);}
xvalue* xl6UdpPacketBytesV1(const void* raw,int64_t size,int64_t offset)
{
    xbytesview view={xrtNetUdpPacketData(raw),xrtNetUdpPacketSize(raw)};
    if(!xl6UdpSliceV1(&view,size,offset)){xl6UdpFailedV1();return NULL;}
    xvalue* result=xrtValueBytes(view);if(!result)xl6UdpFailedV1();return result;
}
static bool packetTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6UdpPacketHandlePolicyV1Get()->Ops&&
        visit(xrtNetUdpPacketOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6UdpPacketHandlePolicyV1Get,xl6UdpPacketDestroyV1,packetTrace)

void xl6UdpErrorPacketDestroyV1(void* raw){xrtNetUdpErrorPacketDestroy(raw);}
uint64_t xl6UdpErrorPacketLengthV1(const void* raw){return xrtNetUdpErrorPacketSize(raw);}
xvalue* xl6UdpErrorPacketBytesV1(const void* raw,int64_t size,int64_t offset)
{
    xbytesview view={xrtNetUdpErrorPacketData(raw),xrtNetUdpErrorPacketSize(raw)};
    if(!xl6UdpSliceV1(&view,size,offset)){xl6UdpFailedV1();return NULL;}
    xvalue* result=xrtValueBytes(view);if(!result)xl6UdpFailedV1();return result;
}
static bool errorTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6UdpErrorPacketHandlePolicyV1Get()->Ops&&
        visit(xrtNetUdpErrorPacketOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6UdpErrorPacketHandlePolicyV1Get,xl6UdpErrorPacketDestroyV1,errorTrace)
