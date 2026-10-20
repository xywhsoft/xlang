#include "xl6_net_proxy_internal.h"
typedef XL6_NetProxyHandshakeV1 Handshake;
void xl6NetProxyHandshakeAppendTextV1(void* raw,xstrview input,int64_t size)
{xl6NetBufferAppendTextV1(&((Handshake*)raw)->input,input,size);}
void xl6NetProxyHandshakeAppendBytesV1(void* raw,const xvalue* input,int64_t size)
{xl6NetBufferAppendBytesV1(&((Handshake*)raw)->input,input,size);}
xvalue* xl6NetProxyHandshakeRemainingV1(void* raw,int64_t size)
{Handshake* h=raw;return xl6NetProxyHandshakeReadyV1(h)?xl6NetBufferReadV1(&h->input,size):NULL;}
xvalue* xl6NetProxyHandshakeOutputV1(void* raw)
{
    Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return NULL;
    xnetspan span={0};xvalue* result=NULL;
    if(xl6NetProxyHandshakeOutputViewV1(h,&span))result=xrtValueBytes((xbytesview){span.Data,span.Size});
    xl6NetBufferLeaveV1(&g);if(!result)xl6NetBufferFailedV1();return result;
}
xvalue* xl6NetProxyHandshakeBoundHostBytesV1(void* raw)
{
    Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return NULL;
    xnetproxyendpoint e={0};xvalue* result=NULL;
    if(xrtNetProxyHandshakeBound(h->native,&e))result=xrtValueBytes((xbytesview){(const uint8_t*)e.Host.Data,e.Host.Size});
    xl6NetBufferLeaveV1(&g);if(!result)xl6NetBufferFailedV1();return result;
}
