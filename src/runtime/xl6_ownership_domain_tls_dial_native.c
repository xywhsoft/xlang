#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtTlsDialRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddNetTimerV1(catalog,xrtTlsDialTimerOwnershipV1Get())&&
        xl6OwnershipCatalogAddDialDoneV1(catalog,xrtTlsDialTransportOwnershipV1Get())&&
        xl6OwnershipCatalogAddTlsStreamEventsV1(catalog,xrtTlsDialStreamOwnershipV1Get())&&
        xl6OwnershipCatalogAddTlsStreamEventsV1(catalog,xrtTlsDialStreamPushOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsDialOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsDialNative)
{
    (void)completion;
    *adapter=xrtTlsDialOwnershipAdapterV1(ref,policies->TlsDialDone,policies->TlsDialDoneCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsDialNative,load,ownershipDomainTlsDialNative)
