#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xrtTlsDialFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtTlsDialFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtTlsDialFuturePayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddTlsDialDoneV1(catalog,xrtTlsDialFutureDialPolicyV1Get())&&
        xl6OwnershipCatalogAddTlsDialDoneV1(catalog,xrtTlsDialFutureDialPushPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsDialFutureOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsDialFutureContextOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsDialFutureNative)
{
    (void)completion;
    *adapter=xrtTlsDialFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtTlsDialFutureContextOwnershipAdapterV1(ref,policies->TlsStreamEvents,policies->TlsStreamEventsCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsDialFutureNative,load,ownershipDomainTlsDialFutureNative)
