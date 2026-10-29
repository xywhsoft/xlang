#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Source/list/producer/watch/result slots remain actual XRT owners. No
 * opaque activation leaves, fabricated holds, or callback-name inference. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetServerFutureOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetServerFutureProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetServerFutureCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetServerFutureStreamPayloadPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetServerFutureNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetServerFutureOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetServerFutureNative,load,ownershipDomainNetServerFutureNative)
