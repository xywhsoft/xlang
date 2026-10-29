#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetProxyDialRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddDialDoneV1(catalog,xrtNetProxyDialTransportOwnershipV1Get())&&
        xl6OwnershipCatalogAddStreamEventsV1(catalog,xrtNetProxyDialStreamOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetProxyDialOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetProxyDialBaseNative)
{
    (void)completion;
    *adapter=xrtNetProxyDialOwnershipAdapterV1(ref,policies->ProxyDialDone,policies->ProxyDialDoneCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetProxyDialBaseNative,load,ownershipDomainNetProxyDialBaseNative)
