#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6WsMessageConfigHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6WsMessageInfoHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6WsMessageStateHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6WsMessageConfigHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6WsMessageInfoHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6WsMessageStateHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(WsMessage,load,NULL)
