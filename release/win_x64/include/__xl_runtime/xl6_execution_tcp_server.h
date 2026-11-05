#ifndef XL6_EXECUTION_TCP_SERVER_H
#define XL6_EXECUTION_TCP_SERVER_H
#include "xl6_execution_resource.h"
struct xnetserverconfig;
struct xnetserver;
struct xnetaddr;
struct xfuture;
/* Same actual source/Engine ownership as Stream and Listener. No callbacks or
 * language records are borrowed by native Server bindings. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpServerCreateV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetserverconfig*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTcpServerFamilyV1Get(void);
/* Prepare does not start Accept; it owns Engine and Server until publication,
 * successful promotion, or actual terminal cleanup. Wrong source is refused. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpServerPrepareAcceptV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTcpServerAcceptSourceForV1(
    XL6_ExecutionResourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpServerPromoteAcceptV1(XL6_ExecutionResourceV1*);
#endif
