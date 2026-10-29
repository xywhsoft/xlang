/* Exact resident capability; selected independently of its producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_network_service.h"

static bool ownershipLoadNetwork(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetResolverFuturePayloadPolicyV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetResolverFutureProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetResolverFutureCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddResolverRequestV1(catalog,xrtNetResolverFutureRequestPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(Network)
{
    (void)completion;
    *adapter=xrtNetResolverFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetResolveOpOwnershipAdapterV1(ref,policies->ResolverRequest,policies->ResolverRequestCount,preparation);
    if(!*adapter)*adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,preparation);
    if(!*adapter)*adapter=xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,preparation);
    if(!*adapter)*adapter=xrtNetAddrListOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Network,ownershipLoadNetwork,ownershipDomainNetwork)
