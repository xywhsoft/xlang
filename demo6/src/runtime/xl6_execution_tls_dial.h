#ifndef XL6_EXECUTION_TLS_DIAL_H
#define XL6_EXECUTION_TLS_DIAL_H
#include "xl6_execution_tls.h"
struct xtlsclientconfig;struct xtlsdialconfig;struct xnetproxy;
/* Prepare owns actual Engine/Resolver and submits nothing. Native constructor
 * receives one Data credit iff accepted; no placeholder native Stream. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTlsPrepareDialV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTlsDialSourceV1(XL6_ExecutionResourceV1*,
    const char*,uint16_t,const struct xtlsclientconfig*,const struct xtlsdialconfig*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTlsProxyDialSourceV1(XL6_ExecutionResourceV1*,
    const struct xnetproxy*,const char*,uint16_t,const struct xtlsclientconfig*,const struct xtlsdialconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionTlsPromoteDialV1(XL6_ExecutionResourceV1*);
#endif
