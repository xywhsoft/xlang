/* The current launcher reserves inside this resident translation unit.
 * Intercept exactly that scheduling boundary for the original host-driven
 * queue tests; all frame implementation, layouts and ownership stay real. */
#define xl6ContinuationExecutorReserveOwned generator_test_reserve
#include "src/runtime/xl6_continuation_frame.c"
