/* Exact policies of this physical producer; never a wildcard admission. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6ProcessPipelineHandlePolicyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ResourceProcessPipelineConfig,load,NULL)
