#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtTlsListenerRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddListenerEventsV1(catalog,xrtTlsListenerTransportOwnershipV1Get())&&
        xl6OwnershipCatalogAddTlsStreamEventsV1(catalog,xrtTlsListenerStreamOwnershipV1Get())&&
        xl6OwnershipCatalogAddTlsStreamEventsV1(catalog,xrtTlsListenerStreamPushOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsListenerOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsListenerNative)
{
    (void)completion;
    *adapter=xrtTlsListenerOwnershipAdapterV1(ref,policies->TlsListenerEvents,policies->TlsListenerEventsCount,preparation);
    if(!*adapter)*adapter=xrtTlsListenerStreamRecordOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsListenerNative,load,ownershipDomainTlsListenerNative)
