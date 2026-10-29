#include "include/xruntime_decl.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6WsExtensionsHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6WsExtensionHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6WsExtensionsHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6WsExtensionHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(WsExtension,load,NULL)
