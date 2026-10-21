#ifndef XL6_EXECUTION_TCP_DIAL_H
#define XL6_EXECUTION_TCP_DIAL_H
#include "xl6_execution_tcp.h"
struct xnetaddrlist;
struct xnetdialconfig;
/* Prepare owns actual parents and submits NOTHING. The caller publishes its
 * prebuilt result class before starting work. A non-NULL Resolver selects the
 * host path; NULL selects the immutable address-list path. No implicit DNS or
 * Engine, raw stream placeholder, second Promise or blocking-pool task. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpPrepareDialV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,XL6_ExecutionResourceV1*);
/* Start exactly once; repeated calls are rejected, never silently reuse an
 * operation with different host/config inputs. Returns one Future reference;
 * the carrier retains the ORIGINAL Future until promotion/true retirement. */
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTcpDialSourceV1(
    XL6_ExecutionResourceV1*,const char*,uint16_t,const struct xnetdialconfig*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTcpDialAddressesSourceV1(
    XL6_ExecutionResourceV1*,const struct xnetaddrlist*,const struct xnetdialconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpPromoteDialV1(XL6_ExecutionResourceV1*);
#endif
