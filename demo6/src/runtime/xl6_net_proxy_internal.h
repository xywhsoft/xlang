#ifndef XL6_NET_PROXY_INTERNAL_H
#define XL6_NET_PROXY_INTERNAL_H
#include "xl6_net_buffer_internal.h"
/* Native proxy storage is immutable, owns copied host/credentials and contains
 * no language/callback references. Handshakes retain it inside XRT, own their
 * input blocks, and never borrow a caller pool or caller input storage. */
typedef struct XL6_NetProxyHandshakeV1 {
    XL6_NetBufferV1 input;
    xnetproxyhandshake* native;
} XL6_NetProxyHandshakeV1;
bool xl6NetProxyInfoV1(const void*,xnetproxyinfo*);
bool xl6NetProxyHandshakeEnterV1(XL6_NetProxyHandshakeV1*,XL6_NetBufferGuardV1*);
bool xl6NetProxyHandshakeReadyV1(XL6_NetProxyHandshakeV1*);
bool xl6NetProxyHandshakeOutputViewV1(XL6_NetProxyHandshakeV1*,xnetspan*);
#endif
