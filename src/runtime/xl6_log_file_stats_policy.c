#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_log.h"

/* Independent xlogfilestats scalar snapshot; it retains no file sink. */
static void destroy(void* payload) { xl6LogFileStatsDestroyV1(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6LogFileStatsHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(LogFileStats,Handle,xl6LogFileStatsHandlePolicyV1Get)
