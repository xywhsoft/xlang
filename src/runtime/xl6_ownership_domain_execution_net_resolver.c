#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_resolver.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionNetResolverFamilyV1Get())&&
    xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutionResourceQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(ExecutionNetResolver)
{
    (void)completion;
    *adapter=xl6ExecutionOwnershipAdapterV1(ref,policies->ExecutionFamily,policies->ExecutionFamilyCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionNetResolver,load,ownershipDomainExecutionNetResolver)
