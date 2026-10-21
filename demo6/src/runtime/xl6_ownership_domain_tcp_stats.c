#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6TcpStreamStatsHandlePolicyV1Get())&&
    xl6OwnershipCatalogAddHandleV1(catalog,xl6TcpListenerStatsHandlePolicyV1Get());}
XL6_OWNERSHIP_DOMAIN(TcpStats,load,NULL)
