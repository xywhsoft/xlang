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
    (void)completion;
    *adapter=xl6TaskPoolOwnershipAdapterV2(ref,policies->TaskPoolExit,policies->TaskPoolExitCount,preparation);
    if(!*adapter)*adapter=xrtTaskPoolOwnershipAdapterV2(ref,policies->TaskPoolExit,policies->TaskPoolExitCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TaskPool,load,ownershipDomainTaskPool)
