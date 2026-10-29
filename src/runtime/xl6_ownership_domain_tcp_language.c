#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6TcpStreamHandlePolicyV1Get())&&
    xl6OwnershipCatalogAddHandleV1(catalog,xl6TcpListenerHandlePolicyV1Get());}
XL6_OWNERSHIP_DOMAIN(TcpLanguage,load,NULL)
