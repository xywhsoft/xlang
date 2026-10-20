#include "xl6_net_proxy_internal.h"
void* xl6NetProxyHandshakeBoundAddressV1(void* raw)
{
    XL6_NetProxyHandshakeV1* h=raw;XL6_NetBufferGuardV1 g={0};void* result=NULL;
    if(!xl6NetProxyHandshakeEnterV1(h,&g))return NULL;
    xnetproxyendpoint e={0};bool ok=xrtNetProxyHandshakeBound(h->native,&e);
    if(ok&&e.Host.Size){xrtSetErrorKind(XERR_NOT_FOUND);ok=false;}
    if(ok)result=xl6NetAddressCopyV1(&e.Address);
    xl6NetBufferLeaveV1(&g);if(!result)xl6NetBufferFailedV1();return result;
}
