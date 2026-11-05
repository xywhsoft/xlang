#ifndef XL6_EXECUTION_TLS_H
#define XL6_EXECUTION_TLS_H
#include "xl6_execution_resource.h"
struct xtlsstream;
struct xtlslistener;
struct xtlsstreamownershipv1;
struct xtlslistenerownershipv1;
struct xnetaddr;
struct xnetstreamconfig;
struct xtlsstreamconfig;
struct XL6_TlsClientContextV1;
struct XL6_TlsServerContextV1;
struct xtlslistenerconfig;
struct xstrview;
struct xfuture;
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTlsConnectV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetaddr*,uint32_t,
    const struct XL6_TlsClientContextV1*,struct xstrview,struct xstrview,
    const struct xtlsstreamconfig*,const struct xnetstreamconfig*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTlsListenV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct XL6_TlsServerContextV1*,
    const struct xtlslistenerconfig*);
/* Private resident factory: borrowed actual owned result from a selected
 * Listener/Dial provider. Same Engine is checked; no default-library raw
 * constructor exposes this path. Success retains one native creator. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTlsAdoptV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,struct xtlsstream*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTlsPrepareAcceptV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTlsAcceptSourceForV1(
    XL6_ExecutionResourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTlsPromoteAcceptV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTlsStreamFamilyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTlsListenerFamilyV1Get(void);
XL6_CODE_LEASE_API const struct xtlsstreamownershipv1* xl6ExecutionTlsStreamDataPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xtlslistenerownershipv1* xl6ExecutionTlsListenerDataPolicyV1Get(void);
#endif
