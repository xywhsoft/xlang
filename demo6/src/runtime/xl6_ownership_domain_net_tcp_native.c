#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
/* Actual source/list/producer/Watch/payload slots. No callback inference,
 * opaque leaves or extra reference counts; native algorithms stay in XRT. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetStreamRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetListenerRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetStreamOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetListenerOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetTcpFutureOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetTcpFutureProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetTcpFutureCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetTcpFutureBytesPayloadPolicyV1Get()) &&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetTcpFutureStreamPayloadPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(NetTcpNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetStreamOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetListenerOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetTcpFutureOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetTcpNative,load,ownershipDomainNetTcpNative)
