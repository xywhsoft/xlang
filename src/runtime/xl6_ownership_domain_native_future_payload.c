/* A native-leaf result has a different payload policy from erased xvalue
 * results. Admit its exact resident policy only when that producer is used;
 * the NativeFuture domain still owns Future preparation and quiescence. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_future_payload.h"

static const struct xfuturepayloadownershipv1* payloadPolicy(void)
{
    return xl6NativeFuturePayloadRuntimeV1Get()->Policy();
}
XL6_OWNERSHIP_DOMAIN_POLICY(NativeFuturePayload,Payload,payloadPolicy)
