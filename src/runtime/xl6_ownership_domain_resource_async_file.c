#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6AsyncFileHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6AsyncFileQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ResourceAsyncFile)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xl6AsyncFileOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ResourceAsyncFile,load,ownershipDomainResourceAsyncFile)
