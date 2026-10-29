#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6WsDeflaterConfigHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6WsDeflaterHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6WsDeflaterConfigHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6WsDeflaterHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(WsDeflater,load,NULL)
