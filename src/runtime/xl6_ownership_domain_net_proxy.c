#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6NetProxyHandlePolicyV1Get());}
XL6_OWNERSHIP_QUERY(NetProxy)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtNetProxyOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetProxy,load,ownershipDomainNetProxy)
