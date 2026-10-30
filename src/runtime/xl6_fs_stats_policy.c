#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"

/* TreeStats and WalkStats are independently allocated xwalkstats snapshots: scalar counters and Stopped only; visitors are never retained. */
static void destroy(void* payload) { xrtFree(payload); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6FsStatsHandlePolicyV1Get,destroy)
XL6_OWNERSHIP_DOMAIN_POLICY(FsStats,Handle,xl6FsStatsHandlePolicyV1Get)
