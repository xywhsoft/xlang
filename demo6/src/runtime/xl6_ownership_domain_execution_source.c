#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_resource.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutionSourceQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(ExecutionSource)
{
    (void)policies;(void)completion;
    *adapter=xl6ExecutionOwnershipAdapterV1(ref,NULL,0,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionSource,load,ownershipDomainExecutionSource)
