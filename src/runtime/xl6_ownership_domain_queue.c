/* Exact resident capability; selected independently of its producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_continuation_queue.h"

XL6_OWNERSHIP_QUERY(Queue)
{
    (void)completion;
    *adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,policies->QueuedStorage,policies->QueuedStorageCount,preparation);
    return *adapter!=NULL;
}
static bool load(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ContinuationQueueQuiescenceV1Get()); }
XL6_OWNERSHIP_DOMAIN(Queue,load,ownershipDomainQueue)
