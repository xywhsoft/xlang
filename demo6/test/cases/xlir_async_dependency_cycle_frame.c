/* The launcher reserves in this resident TU. Keep the original host-driven
 * queue boundary while executing the actual frame implementation. */
#define xl6ContinuationExecutorReserveOwned dependency_reserve
#include "src/runtime/xl6_continuation_frame.c"
