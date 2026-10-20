#ifndef XL6_NET_SOCKET_OWNER_H
#define XL6_NET_SOCKET_OWNER_H
#include "xl6_code_lease.h"
struct xnetsocket_impl;
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipquiescencev1;
typedef struct XL6_NetSocketOwnerV1 XL6_NetSocketOwnerV1;
/* Private resident operation borrow, never a language-visible raw handle.
 * Its actual reference protects the payload; active work is independently BUSY
 * to graph admission. No ownership mutation or metadata lock spans OS I/O. */
typedef struct XL6_NetSocketScopeV1 {
    XL6_NetSocketOwnerV1* owner;
    struct xnetsocket_impl* native;
    uint64_t thread;
    bool control;
} XL6_NetSocketScopeV1;
XL6_CODE_LEASE_API XL6_NetSocketOwnerV1* xl6NetSocketOwnerOpenV1(int family,int kind,bool nonblocking);
XL6_CODE_LEASE_API XL6_NetSocketOwnerV1* xl6NetSocketOwnerEmptyV1(void);
/* Receives into an already empty owner: no post-accept wrapper allocation,
 * no public raw-adopt capability, destination unchanged on AGAIN/failure. */
XL6_CODE_LEASE_API int xl6NetSocketOwnerAcceptV1(XL6_NetSocketOwnerV1*,XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API XL6_NetSocketOwnerV1* xl6NetSocketOwnerRetainV1(XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API void xl6NetSocketOwnerReleaseV1(XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API bool xl6NetSocketOwnerEnterV1(XL6_NetSocketOwnerV1*,XL6_NetSocketScopeV1*);
/* Mutable native metadata (connect, set-option, metadata enablement) must
 * exclude ordinary I/O and each other, not just serialize the final Close. */
XL6_CODE_LEASE_API bool xl6NetSocketOwnerControlV1(XL6_NetSocketOwnerV1*,XL6_NetSocketScopeV1*);
XL6_CODE_LEASE_API void xl6NetSocketOwnerLeaveV1(XL6_NetSocketScopeV1*);
/* Native Close consumes the socket even on error. Clear the live slot BEFORE
 * invoking it, never retry or publish a stale OS handle after failure. */
XL6_CODE_LEASE_API bool xl6NetSocketOwnerCloseV1(XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API bool xl6NetSocketOwnerClosedV1(const XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API struct xrtownershipref xl6NetSocketOwnershipV1(const XL6_NetSocketOwnerV1*);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6NetSocketOwnershipAdapterV1(struct xrtownershipref);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6NetSocketQuiescenceV1Get(void);
#endif
