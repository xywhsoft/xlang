#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xrtTlsListenerFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtTlsListenerFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtTlsListenerFutureStreamPayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsListenerFutureOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsListenerFutureNative)
{
    (void)policies;(void)completion;
    *adapter=xrtTlsListenerFutureOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsListenerFutureNative,load,ownershipDomainTlsListenerFutureNative)
