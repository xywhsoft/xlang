#ifndef XL6_EXECUTION_UDP_H
#define XL6_EXECUTION_UDP_H
#include "xl6_execution_resource.h"
struct xnetaddr;
struct xnetudpconfig;
struct xnetudpstats;
struct xnetdgramcontrol;
struct xnetdgramsend;
struct xnetudpownedmessage;
struct xnetbytesref;
struct xnetspan;
struct xnetbytes;
struct xnetudppacket;
struct xnetudperrorpacket;
struct xfuture;
struct xlir_closure;
/* Retains the existing typed UDP/Engine parent only through acceptance; the
 * shared owned-work producer captures the supplied resident closure. */
XL6_CODE_LEASE_API bool xl6ExecutionUdpPostWorkV1(XL6_ExecutionResourceV1*,struct xlir_closure*);

/* Explicit Source/Engine owners; no module-local callbacks or user Data.
 * This is a resident C service, not a raw-pointer language constructor. */
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionUdpBindV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetaddr*,uint64_t,const struct xnetudpconfig*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionUdpConnectV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetaddr*,uint64_t,const struct xnetudpconfig*);
XL6_CODE_LEASE_API XL6_ExecutionResourceV1* xl6ExecutionUdpOpenV1(
    XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct xnetaddr*,const struct xnetaddr*,uint64_t,const struct xnetudpconfig*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpStateV1(XL6_ExecutionResourceV1*,int*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpLocalV1(XL6_ExecutionResourceV1*,struct xnetaddr*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpPeerV1(XL6_ExecutionResourceV1*,struct xnetaddr*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpConnectedV1(XL6_ExecutionResourceV1*,bool*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpSendControlAvailableV1(XL6_ExecutionResourceV1*,uint32_t*);
XL6_CODE_LEASE_API bool xl6ExecutionUdpStatsV1(XL6_ExecutionResourceV1*,struct xnetudpstats*);

/* Status values are exact native xnetresult values. Typed slices retain
 * owners; Take only transfers on OK. Batch status AND accepted are meaningful.
 * Pointer/descriptor inputs are borrowed only for this physical call. */
XL6_CODE_LEASE_API int xl6ExecutionUdpSendV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,
    const struct xnetdgramcontrol*,const void*,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendVecV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,const struct xnetspan*,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendTakeV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,
    const struct xnetdgramcontrol*,void*,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendBytesV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,
    const struct xnetdgramcontrol*,struct xnetbytes*,size_t,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendPacketV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,
    const struct xnetdgramcontrol*,struct xnetudppacket*,size_t,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendBytesVecV1(XL6_ExecutionResourceV1*,const struct xnetaddr*,
    const struct xnetdgramcontrol*,const struct xnetbytesref*,size_t);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendBatchV1(XL6_ExecutionResourceV1*,const struct xnetdgramsend*,size_t,size_t*);
XL6_CODE_LEASE_API int xl6ExecutionUdpSendOwnedBatchV1(XL6_ExecutionResourceV1*,const struct xnetudpownedmessage*,size_t,size_t*);
/* Owned result, or NULL for empty/error. Language projection must distinguish
 * the checked error context; no borrowed receive view escapes the scope. */
XL6_CODE_LEASE_API struct xnetudppacket* xl6ExecutionUdpReceiveV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xnetudperrorpacket* xl6ExecutionUdpReceiveErrorV1(XL6_ExecutionResourceV1*);

/* Native Future ownership transfers to the caller. The language continuation
 * must independently capture its resident receiver and use typed payload
 * projection; these native results are NOT language class pointers. */
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionUdpWaitAsyncV1(XL6_ExecutionResourceV1*,int);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionUdpWritableAsyncV1(XL6_ExecutionResourceV1*,size_t);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionUdpReceiveAsyncV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionUdpReceiveBatchAsyncV1(XL6_ExecutionResourceV1*,size_t);
XL6_CODE_LEASE_API struct xfuture* xl6ExecutionUdpReceiveErrorAsyncV1(XL6_ExecutionResourceV1*);
XL6_CODE_LEASE_API const struct XL6_ExecutionFamilyV1* xl6ExecutionUdpFamilyV1Get(void);
#endif
