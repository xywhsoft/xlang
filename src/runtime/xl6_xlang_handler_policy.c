#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_xlang_handler.h"
#include "xl6_ownership_domain_impl.h"
#include "src/api/xl_foreign_error.h"
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6XlangHandlerHandlePolicyV1Get,xl6_xlang_handler_destroy,xl6XlangHandlerTraceV1)
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6XlangHandlerHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6XlangModuleQuiescenceV1Get())&&
        xl6OwnershipCatalogSetBoundaryV1(catalog,xl6XlangHandlerBoundaryV1Get());
}
XL6_OWNERSHIP_QUERY(XlangHandler)
{
    (void)policies;
    return xl6XlangHandlerResolveV1(ref,adapter,preparation,completion);
}
XL6_OWNERSHIP_DOMAIN(XlangHandler,load,ownershipDomainXlangHandler)
