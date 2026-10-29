#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Transport graph capabilities without Future/producer dependencies. Event
 * contexts are admitted only by separately selected exact resident policies. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetStreamRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetListenerRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetStreamOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetListenerOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetTcpBaseNative)
{
    (void)completion;
    *adapter=xrtNetStreamOwnershipAdapterV2(ref,policies->StreamEvents,policies->StreamEventsCount,preparation);
    if(!*adapter)*adapter=xrtNetListenerOwnershipAdapterV2(ref,policies->ListenerEvents,policies->ListenerEventsCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetTcpBaseNative,load,ownershipDomainNetTcpBaseNative)
