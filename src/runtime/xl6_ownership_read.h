#ifndef XL6_OWNERSHIP_READ_H
#define XL6_OWNERSHIP_READ_H
#include <stddef.h>
#include <stdint.h>
#include "lib/xrt/xrt.h"
#include "xl6_code_owner.h"
#include "xl6_ownership_read_abi.h"

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
/* Actual resident API implementation; generated programs receive this
 * independent capability through a host binding, not a native source lookup. */
const XL6_OwnershipReadBoundaryV1* xlModuleOwnershipReadBoundaryV1(void);
#endif
