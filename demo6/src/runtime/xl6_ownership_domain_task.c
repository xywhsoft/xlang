/* Exact resident capability; selected independently of its producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_task.h"

static bool ownershipLoadTask(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6TaskPoolQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskPoolOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xl6TaskProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtTaskProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddWatchV1(catalog,xl6TaskWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddTaskV1(catalog,xl6TaskNativeDataPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(Task)
{
    (void)completion;
    *adapter=xl6TaskOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xl6TaskPoolOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtTaskOwnershipAdapterV1(ref,policies->Task,policies->TaskCount,preparation);
    if(!*adapter)*adapter=xrtTaskPoolOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Task,ownershipLoadTask,ownershipDomainTask)
