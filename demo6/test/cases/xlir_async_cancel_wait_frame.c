/* Initial activation now enters through the resident launcher. The test's
 * pending frame inspection still observes the real resident frame. */
#define xl6ContinuationExecutorReserveOwned cancel_wait_reserve
#include "src/runtime/xl6_continuation_frame.c"
