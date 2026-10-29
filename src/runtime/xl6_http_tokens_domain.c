#include "include/xruntime_decl.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6HttpTokensHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6HttpWeightedTokensHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6HttpListHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6HttpTokensHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6HttpWeightedTokensHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6HttpListHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(HttpTokens,load,NULL)
