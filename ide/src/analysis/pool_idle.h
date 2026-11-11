#ifndef XLS_POOL_IDLE_H
#define XLS_POOL_IDLE_H
#include <xrt.h>
#ifdef XLS_COMPILER_XRT
/* Private native-runtime query, not a new XRT ABI. Reuses the matched XRT
 * implementation's synchronized ordinary+finalizer idle predicate while
 * keeping the pool open. The host excludes submissions/retirement and owns
 * the pool throughout. No cache eviction, close, cancellation or join. */
int xlsRuntimeTaskPoolIdle(xtaskpool* pool);
#else
/* The older GUI-owned SDK has no certified open-pool idle query. It must not
 * manufacture a source-reader acknowledgement from public load counters. */
static inline int xlsRuntimeTaskPoolIdle(xtaskpool* pool){(void)pool;return 0;}
#endif
#endif
