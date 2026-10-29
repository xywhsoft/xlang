/* Exact policies of this physical producer; never a wildcard admission. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6PatternBuilderHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6PatternEditHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6PatternKeyHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6PatternPublicationHandlePolicyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ResourcePatternBuilder,load,NULL)
