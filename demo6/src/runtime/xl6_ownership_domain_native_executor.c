/* Independent native capability: never pulled in by default async execution. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"

static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtExecutorOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtExecutorJobOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NativeExecutor)
{
    (void)completion;
    *adapter=xrtExecutorOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtExecutorJobOwnershipAdapterV1(ref,
        policies->Executor,policies->ExecutorCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NativeExecutor,load,ownershipDomainNativeExecutor)
