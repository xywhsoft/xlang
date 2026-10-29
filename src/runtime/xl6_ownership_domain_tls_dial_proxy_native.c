#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Proxy is an independent capability, not an unconditional side effect of
 * selecting direct Dial or its Future. Query is supplied by the base domain. */
XL6_OWNERSHIP_DOMAIN_POLICY(TlsDialProxyNative,ProxyDialDone,xrtTlsDialProxyTransportOwnershipV1Get)
