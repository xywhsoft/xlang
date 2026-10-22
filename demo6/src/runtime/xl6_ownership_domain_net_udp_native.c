#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetUdpFutureOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetUdpFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetUdpFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetUdpFuturePacketPayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetUdpFutureErrorPayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetUdpFutureBatchPayloadPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetUdpNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetUdpFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetUdpBatchOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetUdpNative,load,ownershipDomainNetUdpNative)
