#include "xl6_net_proxy_internal.h"
xstrview xl6NetProxyHandshakeBoundHostV1(void* raw)
{
    XL6_NetProxyHandshakeV1* h=raw;XL6_NetBufferGuardV1 g={0};xstrview result={0};
    if(!xl6NetProxyHandshakeEnterV1(h,&g))return result;
    xnetproxyendpoint e={0};char address[80];bool ok=xrtNetProxyHandshakeBound(h->native,&e);
    if(ok){xstrview host=e.Host;
        if(!host.Size){size_t count=xrtNetAddrText(&e.Address,address,sizeof(address));host=(xstrview){address,count};ok=count!=0;}
        if(ok&&xl6TextSelect(&host,-1))result=xl6TextCopy(host);}
    xl6NetBufferLeaveV1(&g);if(!result.Data)xl6NetBufferFailedV1();return result;
}
