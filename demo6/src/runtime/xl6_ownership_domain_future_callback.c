/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_future_callback.h"

static bool ownershipLoadFutureCallback(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xl6FutureCallbackProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddWatchV1(catalog,xl6FutureCallbackWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xl6FutureCallbackCancelPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(FutureCallback)
{
    (void)policies;(void)completion;
    *adapter=xl6FutureCallbackOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(FutureCallback,ownershipLoadFutureCallback,ownershipDomainFutureCallback)
