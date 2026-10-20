#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_engine.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionNetEngineFamilyV1Get())&&
    xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutionResourceQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(ExecutionNetEngine)
{
    (void)completion;
    *adapter=xl6ExecutionOwnershipAdapterV1(ref,policies->ExecutionFamily,policies->ExecutionFamilyCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionNetEngine,load,ownershipDomainExecutionNetEngine)
