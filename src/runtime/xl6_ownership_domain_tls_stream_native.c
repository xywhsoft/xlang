#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Session/configuration lifetime belongs to the actual owned Data graph.
 * Admit only permanent policies populated by the selected resident producer. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtTlsStreamRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddStreamEventsV1(catalog,xrtTlsStreamTransportOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsStreamOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsStreamNative)
{
    (void)completion;
    *adapter=xrtTlsStreamOwnershipAdapterV1(ref,policies->TlsStreamEvents,policies->TlsStreamEventsCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsStreamNative,load,ownershipDomainTlsStreamNative)
