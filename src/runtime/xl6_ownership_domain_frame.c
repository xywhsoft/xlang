/* Exact resident capability; selected independently of its producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_continuation_future.h"

static bool ownershipLoadFrame(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xl6ContinuationProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddWatchV1(catalog,xl6ContinuationWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddQueuedStorageV1(catalog,xl6ContinuationProducedStorageV2Get());
}
XL6_OWNERSHIP_QUERY(Frame)
{
    (void)policies;(void)completion;
    *adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Frame,ownershipLoadFrame,ownershipDomainFrame)
