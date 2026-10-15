#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddHandleV1(catalog,xl6ProcessRunOptionsHandlePolicyV1Get()); }
XL6_OWNERSHIP_QUERY(ResourceProcessOptions)
{
    (void)completion;
    /* Exact V2 admission checks any observer against the sealed population.
     * It refuses legacy/unknown callbacks before tracing their Data and never
     * requests cancellation in order to make retirement appear possible. */
    *adapter=xrtCancelOwnershipAdapterV2(ref,policies->CancelWatch,policies->CancelWatchCount,preparation);
    if(!*adapter)*adapter=xrtCancelWatchOwnershipAdapterV1(ref,policies->CancelWatch,policies->CancelWatchCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ResourceProcessOptions,load,ownershipDomainResourceProcessOptions)
