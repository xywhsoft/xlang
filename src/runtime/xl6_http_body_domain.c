#include "include/xruntime_decl.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
extern const XL6_NativeHandlePolicyV1* xl6HttpBodyPlanHandlePolicyV2Get(void);
extern const XL6_NativeHandlePolicyV1* xl6Http1BodyReaderHandlePolicyV2Get(void);
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6HttpBodyPlanHandlePolicyV2Get()) && xl6OwnershipCatalogAddHandleV1(catalog,xl6Http1BodyReaderHandlePolicyV2Get());}
XL6_OWNERSHIP_DOMAIN(HttpBody,load,NULL)
