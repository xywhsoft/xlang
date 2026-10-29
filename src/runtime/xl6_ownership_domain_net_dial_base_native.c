#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
/* Core Dial capabilities, without selecting TCP Future. The older composite
 * NetDialNative domain remains valid: the catalog deduplicates exact policies
 * and requires identical adapters/preparations on overlapping queries. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetDialRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddResolverRequestV1(catalog,xrtNetDialResolverPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetDialOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetResolverOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetResolveOpOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetDialBaseNative)
{
    (void)completion;
    *adapter=xrtNetDialOwnershipAdapterV1(ref,policies->DialDone,policies->DialDoneCount,preparation);
    if(!*adapter)*adapter=xrtNetResolveOpOwnershipAdapterV1(ref,policies->ResolverRequest,policies->ResolverRequestCount,preparation);
    if(!*adapter)*adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,preparation);
    if(!*adapter)*adapter=xrtNetAddrListOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetDialBaseNative,load,ownershipDomainNetDialBaseNative)
