/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_future_aggregate.h"

static bool ownershipLoadFutureAggregate(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddPayloadV1(catalog,xrtFutureCombineAllPayloadPolicyV1Get()) &&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtFutureCombinePickPayloadPolicyV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtFutureCombineProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddProjectedWatchV1(catalog,xrtFutureCombineWatchPolicyV2Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtFutureCombineCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddCombineV1(catalog,xl6FutureAggregatePolicyV1Get());
}
XL6_OWNERSHIP_QUERY(FutureAggregate)
{
    (void)completion;
    *adapter=xl6FutureAggregateOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtFutureCombineOwnershipAdapterV1(ref,policies->Combine,policies->CombineCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(FutureAggregate,ownershipLoadFutureAggregate,ownershipDomainFutureAggregate)
