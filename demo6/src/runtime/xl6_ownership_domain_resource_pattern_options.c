/* Exact policies of this physical producer; never a wildcard admission. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6PatternOptionsHandlePolicyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ResourcePatternOptions,load,NULL)
