#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
#include "xl6_promise.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6PromiseHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6PromiseQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(Promise)
{
    (void)policies;(void)preparation;
    *adapter=xl6PromiseOwnershipAdapterV1(ref,completion);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Promise,load,ownershipDomainPromise)
