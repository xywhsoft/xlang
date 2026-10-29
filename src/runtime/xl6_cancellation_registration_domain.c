#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
#include "xl6_cancellation.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog, xl6CancellationRegistrationHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog, xl6CancellationWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog, xl6CancellationRegistrationQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(CancellationRegistration)
{
    (void)policies; (void)preparation;
    *adapter=xl6CancellationRegistrationAdapterV1(ref,completion); return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(CancellationRegistration,load,ownershipDomainCancellationRegistration)
