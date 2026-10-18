/* Independent capability: native inline Future adapters need no queue policy. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_future_callback.h"

static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xl6FutureCallbackProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddWatchV1(catalog,xl6FutureCallbackQueuedWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddQueuedStorageV1(catalog,xl6FutureCallbackQueuedStorageV2Get());
}
XL6_OWNERSHIP_QUERY(FutureCallbackQueue)
{
    (void)policies;(void)completion;
    *adapter=xl6FutureCallbackOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(FutureCallbackQueue,load,ownershipDomainFutureCallbackQueue)
