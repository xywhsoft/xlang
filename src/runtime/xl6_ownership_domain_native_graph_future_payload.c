#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_graph_future_payload.h"
static const xfuturepayloadownershipv1* payloadPolicy(void)
{ return xl6NativeGraphFuturePayloadRuntimeV1Get()->Policy(); }
XL6_OWNERSHIP_DOMAIN_POLICY(NativeGraphFuturePayload,Payload,payloadPolicy)
