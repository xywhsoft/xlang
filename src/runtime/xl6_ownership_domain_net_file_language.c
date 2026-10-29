#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6NetFileHandlePolicyV1Get());}
XL6_OWNERSHIP_DOMAIN(NetFileLanguage,load,NULL)
