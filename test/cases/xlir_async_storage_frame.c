/* Route initial ready admission at its resident implementation boundary.
 * All allocation, producer binding, publication and retirement stay real. */
#ifdef XLIR_ASYNC_MANAGED_PROBE
#define xl6ContinuationExecutorReserveOwned async_managed_reserve
#else
#define xl6ContinuationExecutorReserveOwned async_storage_reserve
#endif
#include "src/runtime/xl6_continuation_frame.c"
