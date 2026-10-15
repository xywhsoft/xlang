/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_continuation_queue.h"

XL6_OWNERSHIP_QUERY(Queue)
{
    (void)completion;
    *adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,policies->QueuedStorage,policies->QueuedStorageCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Queue,NULL,ownershipDomainQueue)
