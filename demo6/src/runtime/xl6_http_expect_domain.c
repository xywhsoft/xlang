#include "include/xruntime_decl.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6HttpExpectationsHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6HttpExpectationsHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(HttpExpect,load,NULL)
