#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Only exact resident policies. Inner TCP Dial and protocol event bindings are
 * separate real owners, not opaque leaves hidden by the outer Future. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetProxyDialRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetDialRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddResolverRequestV1(catalog,xrtNetDialResolverPolicyV1Get())&&
        xl6OwnershipCatalogAddDialDoneV1(catalog,xrtNetProxyDialTransportOwnershipV1Get())&&
        xl6OwnershipCatalogAddProxyDialDoneV1(catalog,xrtNetProxyDialFutureDialPolicyV1Get())&&
        xl6OwnershipCatalogAddStreamEventsV1(catalog,xrtNetProxyDialStreamOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetDialOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetProxyDialOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetProxyDialFutureOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetProxyDialFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetProxyDialFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetProxyDialFuturePayloadPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetProxyDialNative)
{
    (void)completion;
    *adapter=xrtNetProxyDialFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetProxyDialOwnershipAdapterV1(ref,policies->ProxyDialDone,policies->ProxyDialDoneCount,preparation);
    if(!*adapter)*adapter=xrtNetDialOwnershipAdapterV1(ref,policies->DialDone,policies->DialDoneCount,preparation);
    if(!*adapter)*adapter=xrtNetStreamOwnershipAdapterV2(ref,policies->StreamEvents,policies->StreamEventsCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetProxyDialNative,load,ownershipDomainNetProxyDialNative)
