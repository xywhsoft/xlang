#ifndef XL_FOREIGN_GRAPH_H
#define XL_FOREIGN_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Include the selected XRT declarations before this header. */
struct xvalue;
struct XL6_NativeHandlePolicyV1;
struct xnetworker;
struct xnettaskownershipv1;
#ifndef XL6_FOREIGN_GRAPH_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FOREIGN_GRAPH_API __declspec(dllexport)
#else
#define XL6_FOREIGN_GRAPH_API
#endif
#endif

/* Explicit native composition contract, not permission to inspect arbitrary
 * handles. Trace enumerates EACH actual owning slot (including native XRT
 * references); every child still requires independent catalog admission.
 * Trace runs read-only under whole-domain Freeze, with no allocation, locks,
 * callbacks into the language, or reference transitions. All raw mutations
 * occur inside Enter/Leave, including callback-reentrant calls (which refuse).
 * Prepare is optional, runs outside Freeze, and obeys XRT's preparation ABI.
 * The terminal releaser is a checked point action. It owns final destruction,
 * including native child releases, and runs at most once while its captures
 * are intact. Its owned callable MUST keep this descriptor's callbacks and
 * their transitive native code resident. A language xcall in the same linked
 * artifact supplies that capability; a borrowed function pointer does not.
 * Create copies this descriptor. Failure never consumes raw. */
typedef struct XL6_ForeignGraphDescriptorV1 {
    size_t size;
    uint64_t abiTag;
    bool (*Trace)(const void*,xrtownershipvisitor,void*);
    xrtownershipprepareresult (*Prepare)(void*);
    /* Optional native owned task. It settles its own completion/error carrier;
     * it must not unwind across C. The task owner retains this record until
     * AFTER Task returns, so its code dependency cannot disappear on its tail. */
    void (*Task)(struct xnetworker*,void*);
} XL6_ForeignGraphDescriptorV1;
#define XL6_FOREIGN_GRAPH_V1_TAG UINT64_C(0x584C364647524131)
typedef struct XL6_ForeignGraphScopeV1 { void* record; void* raw; } XL6_ForeignGraphScopeV1;

XL6_FOREIGN_GRAPH_API void* xl6ForeignGraphCreateV1(void*,const XL6_ForeignGraphDescriptorV1*,const struct xvalue*);
XL6_FOREIGN_GRAPH_API void xl6ForeignGraphDestroyV1(void*);
XL6_FOREIGN_GRAPH_API bool xl6ForeignGraphEnterV1(void*,XL6_ForeignGraphScopeV1*);
XL6_FOREIGN_GRAPH_API void xl6ForeignGraphLeaveV1(XL6_ForeignGraphScopeV1*);
XL6_FOREIGN_GRAPH_API const struct XL6_NativeHandlePolicyV1* xl6ForeignGraphHandlePolicyV1Get(void);
XL6_FOREIGN_GRAPH_API const struct xnettaskownershipv1* xl6ForeignGraphTaskPolicyV1Get(void);
#endif
