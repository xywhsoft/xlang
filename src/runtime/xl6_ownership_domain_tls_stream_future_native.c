#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xrtTlsStreamFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtTlsStreamFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtTlsStreamFutureBytesPayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsStreamFutureOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsStreamFutureNative)
{
    (void)policies;(void)completion;
    *adapter=xrtTlsStreamFutureOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsStreamFutureNative,load,ownershipDomainTlsStreamFutureNative)
