/* Exact audited capability; no admission for handlers or arbitrary payloads. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"

XL6_OWNERSHIP_DOMAIN_POLICY(XlangContext,Handle,xl6XlangContextHandlePolicyV1Get)
