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
typedef struct XL6_NetworkServiceRuntimeV1 {
    bool (*Initialize)(XL6_NetworkServices*);
    /* wait=false is a single nonwaiting step. wait=true retains the legacy
     * checked boundary's synchronous policy, but never drops failed slots. */
    XL6_ModuleRetireResult (*Retire)(XL6_NetworkServices*, bool wait);
    bool (*IsEmpty)(const XL6_NetworkServices*);
} XL6_NetworkServiceRuntimeV1;
XL6_CODE_LEASE_API const XL6_NetworkServiceRuntimeV1* xl6NetworkServiceRuntimeV1Get(void);
#endif
