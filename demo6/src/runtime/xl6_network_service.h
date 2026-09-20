#ifndef XL6_NETWORK_SERVICE_H
#define XL6_NETWORK_SERVICE_H
#include "xl6_code_lease.h"
struct xnetengine;
struct xnetresolver;

/* Embedded owning slots, serialized by the module lifecycle driver. Borrowed
 * native handles do not certify the opaque service's complete ownership graph. */
typedef enum XL6_NetworkServicePhase {
    XL6_NETWORK_SERVICE_EMPTY = 0,
    XL6_NETWORK_SERVICE_STARTING,
    XL6_NETWORK_SERVICE_OPEN,
    XL6_NETWORK_SERVICE_RETIRING
} XL6_NetworkServicePhase;
typedef struct XL6_NetworkServices {
    struct xnetengine* engine;
    struct xnetresolver* resolver;
    XL6_NetworkServicePhase phase;
} XL6_NetworkServices;
typedef bool (*XL6_NetworkServiceOwnershipTrace)(const XL6_NetworkServices*,
    XL6_ModuleOwnershipVisit, void*);
typedef struct XL6_NetworkServiceRuntimeV1 {
    bool (*Initialize)(XL6_NetworkServices*);
    /* wait=false is a single nonwaiting step. wait=true retains the legacy
     * checked boundary's synchronous policy, but never drops failed slots. */
    XL6_ModuleRetireResult (*Retire)(XL6_NetworkServices*, bool wait);
    bool (*IsEmpty)(const XL6_NetworkServices*);
} XL6_NetworkServiceRuntimeV1;
XL6_CODE_LEASE_API const XL6_NetworkServiceRuntimeV1* xl6NetworkServiceRuntimeV1Get(void);

/* V1 remains an immutable compatibility ABI for already-generated artifacts.
 * V2 adds the missing physical ownership view. Trace runs only under the host's
 * whole-domain freeze and reports each installed creator slot exactly once;
 * child admission/Count/Trace is still the ownership planner's responsibility. */
typedef struct XL6_NetworkServiceRuntimeV2 {
    bool (*Initialize)(XL6_NetworkServices*);
    XL6_ModuleRetireResult (*Retire)(XL6_NetworkServices*, bool wait);
    bool (*IsEmpty)(const XL6_NetworkServices*);
    XL6_NetworkServiceOwnershipTrace Trace;
} XL6_NetworkServiceRuntimeV2;
XL6_CODE_LEASE_API const XL6_NetworkServiceRuntimeV2* xl6NetworkServiceRuntimeV2Get(void);
#endif
