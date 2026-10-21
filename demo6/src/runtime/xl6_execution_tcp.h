#ifndef XL6_EXECUTION_TCP_H
#define XL6_EXECUTION_TCP_H
#include "xl6_execution_resource.h"
struct xnetaddr;
struct xnetstream;
struct xnetlistener;
struct xnetstreamconfig;
struct xnetlistenconfig;
struct xnetstreamstats;
struct xnetlistenerstats;
struct xfuture;
/* Source/parent are real language owners. Native Data/callback fields stay
 * empty. An accepted Stream is borrowed and retained transactionally; it must
 * belong to exactly this Engine. No public raw-pointer constructor. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpConnectV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetaddr*,uint32_t,const struct xnetstreamconfig*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpListenV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetlistenconfig*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpAdoptV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,struct xnetstream*);
/* Private prebuilt result carrier: Prepare owns real Engine/Listener owners,
 * but submits no operation. AcceptSource starts at most one real Accept after
 * class publication, returning a new Future reference on every successful
 * call. The carrier retains it until promotion or terminal cleanup. No fake
 * Stream pointer, and no constructor rollback that abandons in-flight Accept.
 * Language wrappers keep the class private until continuation publication. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionTcpPrepareAcceptV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionTcpAcceptSourceV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpPromoteAcceptV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpStateV1(XL6_ExecutionResourceV1*,int*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpLocalV1(XL6_ExecutionResourceV1*,struct xnetaddr*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpRemoteV1(XL6_ExecutionResourceV1*,struct xnetaddr*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpShutdownWriteV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpPauseReadV1(XL6_ExecutionResourceV1*,bool);
XL6_CODE_LEASE_API bool xl6ExecutionTcpStreamStatsV1(XL6_ExecutionResourceV1*,struct xnetstreamstats*);
XL6_CODE_LEASE_API bool xl6ExecutionTcpListenerStatsV1(XL6_ExecutionResourceV1*,struct xnetlistenerstats*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTcpStreamFamilyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionTcpListenerFamilyV1Get(void);
#endif
