#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Opt-in Dial domain. Engine/Resolver/TCP-only products do not select it.
 * Every edge is a real native slot; no callback inference or opaque leaves. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetDialRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddResolverRequestV1(catalog,xrtNetDialResolverPolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetDialOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetDialFutureOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetDialFutureProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetDialFutureCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddDialDoneV1(catalog,xrtNetDialFutureDialPolicyV1Get()) &&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetDialFuturePayloadPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetDialNative)
{
    (void)completion;
    *adapter=xrtNetDialFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetDialOwnershipAdapterV1(ref,policies->DialDone,policies->DialDoneCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetDialNative,load,ownershipDomainNetDialNative)
