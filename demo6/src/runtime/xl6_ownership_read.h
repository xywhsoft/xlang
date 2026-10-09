#ifndef XL6_OWNERSHIP_READ_H
#define XL6_OWNERSHIP_READ_H
#include <stddef.h>
#include <stdint.h>
#include "lib/xrt/xrt.h"
#include "xl6_code_owner.h"

/* Compiler-private resident host capability. Begin excludes host entry and
 * retirement BEFORE a nonblocking XRT Freeze attempt; Admits runs only under
 * that Freeze and must reject unknown participants BEFORE Count/Trace.
 * The current synchronous native caller is paused, not removed as a root.
 * Every other generated execution must be excluded independently. No claim,
 * cancellation, drain, semantic callback, inferred leaf or RC discount.
 * Begin/End allocate nothing and preserve diagnostics. BUSY/ERROR leave the
 * caller's zero-initialized scope empty. End follows Freeze end, before any
 * cleanup, clone or generated code. A live scope is same-thread, uncopyable.
 * This is NOT the lifecycle/retirement adapter or a public language facility. */
typedef struct XL6_OwnershipReadScopeV1 {
    const void* self;
    unsigned long long thread;
} XL6_OwnershipReadScopeV1;
enum { XL6_OWNERSHIP_READ_ERROR_V1=-1, XL6_OWNERSHIP_READ_BUSY_V1=0,
    XL6_OWNERSHIP_READ_READY_V1=1 };
typedef struct XL6_OwnershipReadBoundaryV1 {
    size_t size;
    int (*Begin)(XL6_OwnershipReadScopeV1*);
    bool (*Admits)(const XL6_OwnershipReadScopeV1*,xrtownershipref);
    void (*End)(XL6_OwnershipReadScopeV1*);
} XL6_OwnershipReadBoundaryV1;
/* Separate opt-in artifact export. Exact tag certifies that EVERY generated
 * execution touching this module's owning slots uses a tracked host entry or
 * resident CodeScope, including direct exports, foreign callbacks and async
 * tails. Unknown/uninstrumented native writers cannot advertise this contract.
 * trace is the SAME complete global/service trace as the CodeOwner graph.
 * No tag-only best effort for old artifacts or opaque GetSymbol invocation. */
typedef struct XL6_ModuleReadContractV1 {
    size_t size;
    unsigned long long abiTag;
    XL6_ModuleOwnershipTrace trace;
} XL6_ModuleReadContractV1;
typedef const XL6_ModuleReadContractV1* (*XL6_ModuleReadContractGetV1)(void);
#endif
