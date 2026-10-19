#include "xl6_execution_resource_internal.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionTaskPoolFamilyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutionResourceQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ExecutionPool)
{
    (void)completion;
    *adapter=xl6ExecutionOwnershipAdapterV1(ref,policies->ExecutionFamily,policies->ExecutionFamilyCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionPool,load,ownershipDomainExecutionPool)
