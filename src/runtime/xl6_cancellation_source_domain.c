#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_ownership_cancel.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog, xl6CancellationSourceHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog, xl6CancellationTokenHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog, xrtCancelOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog, xrtCancelWatchOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(CancellationSource)
{
    (void)preparation;
    return xl6OwnershipResolveCancelV1(ref, policies, adapter, completion);
}
XL6_OWNERSHIP_DOMAIN(CancellationSource,load,ownershipDomainCancellationSource)
