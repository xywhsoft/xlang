#ifndef XL6_CODE_OWNER_H
#define XL6_CODE_OWNER_H
#include <stddef.h>
#include <stdbool.h>

/* Host-owned code lifetime capability, bound before module initialization.
 * A successful acquire pins code and dependency libraries; closing rejects
 * new acquisitions. Accepted continuations transfer the same pin to their
 * successor. Release must be called by resident runtime code AFTER the last
 * generated callback (including physical frame release) has returned. It may
 * destroy the module, so no generated stack or descriptor access may follow.
 * Hooks return normally and preserve the execution context's diagnostic.
 * This is not a cancellation or completion token. */
/* One token definition for the resident ABI and its standalone generated C
 * declaration. Keep generated artifacts independent of source-tree headers. */
#define XL6_CODE_OWNER_DECLARATION \
typedef struct XL6_CodeOwner { \
    size_t size; \
    void* context; \
    int (*acquire)(void*); \
    void (*release)(void*); \
} XL6_CodeOwner;
#ifndef XL6_CODE_OWNER_TYPE_DEFINED
#define XL6_CODE_OWNER_TYPE_DEFINED
XL6_CODE_OWNER_DECLARATION
#endif
/* Additive, tagged extension. The four-field owner remains the stable prefix;
 * old owners (including unknown extensions) remain opaque to graph inspection.
 * Ops describes the SAME physical node whose count acquire/release changes,
 * using base.context as its canonical identity. It must include every owning
 * child, not just executable addresses. The descriptor is borrowed and copied
 * before publication. Its code must remain resident through the last release.
 * These read-only views require caller-provided whole-graph quiescence. */
/* 声明会字符串化到 C/C++ 公共头；用 0 避免固化宿主 C 的 NULL 宏展开。 */
#define XL6_CODE_OWNER_GRAPH_DECLARATION \
struct xrtownershipops; \
typedef struct XL6_CodeOwnerGraph { \
    XL6_CodeOwner base; \
    unsigned long long abiTag; \
    const struct xrtownershipops* ops; \
} XL6_CodeOwnerGraph; \
static inline const struct xrtownershipops* xl6CodeOwnerGraphOps(const XL6_CodeOwner* owner) \
{ \
    const XL6_CodeOwnerGraph* graph; \
    if (owner == 0 || owner->size < sizeof(XL6_CodeOwnerGraph)) return 0; \
    graph = (const XL6_CodeOwnerGraph*)owner; \
    return graph->abiTag == 0x584C36434F574E31ULL ? graph->ops : 0; \
}
#ifndef XL6_CODE_OWNER_GRAPH_TYPE_DEFINED
#define XL6_CODE_OWNER_GRAPH_TYPE_DEFINED
XL6_CODE_OWNER_GRAPH_DECLARATION
#endif
#define XL6_MODULE_OWNERSHIP_DECLARATION \
struct xrtownershipref; \
typedef bool (*XL6_ModuleOwnershipVisit)(struct xrtownershipref, void*); \
typedef bool (*XL6_ModuleOwnershipTrace)(XL6_ModuleOwnershipVisit, void*); \
typedef struct XL6_ModuleRetirementV1 { \
    size_t size; \
    unsigned long long abiTag; \
    XL6_ModuleOwnershipTrace trace; \
    int (*finish)(void); \
} XL6_ModuleRetirementV1; \
typedef const XL6_ModuleRetirementV1* (*XL6_ModuleRetirementGet)(void); \
typedef enum XL6_ModulePrepareResult { \
    XL6_MODULE_PREPARE_FAILED = -1, \
    XL6_MODULE_PREPARE_BUSY = 0, \
    XL6_MODULE_PREPARE_READY = 1 \
} XL6_ModulePrepareResult; \
typedef struct XL6_ModuleRetirementV2 { \
    size_t size; \
    unsigned long long abiTag; \
    const XL6_ModuleRetirementV1* graph; \
    XL6_ModulePrepareResult (*prepare)(void); \
} XL6_ModuleRetirementV2; \
typedef const XL6_ModuleRetirementV2* (*XL6_ModuleRetirementGetV2)(void); \
typedef struct XL6_ModuleRetirementV3 { \
    size_t size; \
    unsigned long long abiTag; \
    const XL6_ModuleRetirementV1* graph; \
    XL6_ModulePrepareResult (*preflight)(void); \
} XL6_ModuleRetirementV3; \
typedef const XL6_ModuleRetirementV3* (*XL6_ModuleRetirementGetV3)(void);
#ifndef XL6_MODULE_OWNERSHIP_TYPE_DEFINED
#define XL6_MODULE_OWNERSHIP_TYPE_DEFINED
XL6_MODULE_OWNERSHIP_DECLARATION
#endif
/* Independent from V1's bool Finish and V2/V3 preparation. BUSY is not a
 * semantic failure and never authorizes releasing executable storage. A
 * FAILED step retains the module for retry, even without an allocated error.
 * Failures are events: callers preserve the first failure across later READY.
 * The language finalizer runs at most once per successful initialization. */
typedef enum XL6_ModuleRetireResult {
    XL6_MODULE_RETIRE_FAILED = -1,
    XL6_MODULE_RETIRE_BUSY = 0,
    XL6_MODULE_RETIRE_READY = 1
} XL6_ModuleRetireResult;
typedef struct XL6_ModuleRetirementV4 {
    size_t size;
    unsigned long long abiTag;
    const XL6_ModuleRetirementV1* graph;
    XL6_ModulePrepareResult (*preflight)(void);
    XL6_ModuleRetireResult (*step)(void);
} XL6_ModuleRetirementV4;
typedef const XL6_ModuleRetirementV4* (*XL6_ModuleRetirementGetV4)(void);
/* Separate opt-in export; never extend or reinterpret CodeOwnerGraph's tail.
 * Exact tag 0x584C364D525431ULL certifies verified cleanup instructions and
 * resident physical storage, NOT arbitrary children. Host admission must
 * validate every node and exclude entries before trace. finish is invoked
 * only after all semantic finalizers and a revalidated whole-graph Clear,
 * while actual code/field pins remain alive. Unknown services/owners refuse. */
/* V2 composes the unchanged V1 graph contract, exported separately with tag
 * 0x584C364D525432ULL. prepare stops resource admission and drains accepted
 * work; it must NOT release module globals, invoke their semantic finalizers,
 * or change the initialized phase. READY is persistent until final teardown.
 * Do not wait for an active producer that may depend on the retirement caller.
 * BUSY preserves live resources for retry, FAILED is independent of an error
 * allocation and also retryable. Already drained resources stay drained.
 * The host holds a real module reference and excludes competing retirement
 * drivers, but invokes prepare OUTSIDE its API lock and ownership admission.
 * Reentry may request retirement, not finish/delete any pinned module. */
/* V3 uses tag 0x584C364D525433ULL and the unchanged V1 graph. Unlike V2,
 * preflight READY only certifies initialization and unmodelled service
 * readiness. It does NOT certify that graph-owned work has drained, stop
 * its execution, or authorize its cancellation. Every modelled service is
 * traced through real owning slots and independently admitted/prepared by
 * the host's exact graph adapters; unknown services still refuse. V3 and V2
 * are mutually exclusive exports, not interchangeable readiness meanings.
 * The same real host pin, entry exclusion, error outcome and outside-lock
 * invocation rules apply. Globals/finalizers remain untouched by preflight.
 * An unmodelled pool must keep admission open while accepted modelled work
 * can still submit to it. Such mixed modules report BUSY before touching
 * the pool until that work drains; this is not a mixed-cycle solution.
 * Normal checked teardown must still quiesce all services before globals. */
typedef int (*XL6_BindCodeOwnerProc)(const XL6_CodeOwner*);
#endif
