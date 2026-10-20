#include "xl6_net_proxy_internal.h"
typedef XL6_NetProxyHandshakeV1 Handshake;
void xl6NetProxyHandshakeAppendBufferV1(void* raw,void* input,int64_t size)
{xl6NetBufferAppendBufferV1(&((Handshake*)raw)->input,input,size);}
int64_t xl6NetProxyHandshakeOutputIntoV1(void* raw,void* target,int64_t size)
{
    xbuffer* output=xl6BufferChecked(target);if(!output){xl6NetBufferFailedV1();return 0;}
    Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return 0;
    xnetspan span={0};bool ok=xl6NetProxyHandshakeOutputViewV1(h,&span);
    xbytesview selected={span.Data,span.Size};if(ok)ok=xl6BufferSelect(&selected,size);
    if(ok)ok=xl6BufferAllocation(xrtBufferAppend(output,selected));
    xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return ok?(int64_t)selected.Size:0;
}
