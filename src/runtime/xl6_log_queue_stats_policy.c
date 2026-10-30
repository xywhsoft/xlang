#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_log.h"

/* Independent fixed array of scalar queue counters; it retains no queue, worker or target sink. */
static void destroy(void* payload) { xl6LogQueueStatsDestroyV1(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6LogQueueStatsHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(LogQueueStats,Handle,xl6LogQueueStatsHandlePolicyV1Get)
