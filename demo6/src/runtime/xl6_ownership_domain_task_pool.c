/* Control/lifetime capability, independent of language worker frame layouts. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_task.h"

static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6TaskPoolQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskPoolOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TaskPool)
{
    (void)policies;(void)completion;
    *adapter=xl6TaskPoolOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtTaskPoolOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TaskPool,load,ownershipDomainTaskPool)
