/* Exact status-payload capability, selected only with Process.waitAsync. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
XL6_OWNERSHIP_DOMAIN_POLICY(ProcessFuture,Payload,xrtProcessFuturePayloadPolicyV1Get)
