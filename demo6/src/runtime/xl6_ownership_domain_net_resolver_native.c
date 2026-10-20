#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetResolverOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetResolveOpOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetResolverFutureOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetResolverFuturePayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetResolverFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetResolverFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddResolverRequestV1(catalog,xrtNetResolverFutureRequestPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetResolverNative)
{
    (void)completion;
    *adapter=xrtNetResolverFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetResolveOpOwnershipAdapterV1(ref,policies->ResolverRequest,policies->ResolverRequestCount,preparation);
    if(!*adapter)*adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,preparation);
    if(!*adapter)*adapter=xrtNetAddrListOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetResolverNative,load,ownershipDomainNetResolverNative)
