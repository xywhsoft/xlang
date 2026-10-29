/* Only the transport TU redirects WatchAdd; the contract still calls the
 * actual XRT registration API after its deliberate transport rejection. */
#define xrtFutureWatchAdd handoff_watch_add
#include "../../src/runtime/xl6_continuation_future.c"
