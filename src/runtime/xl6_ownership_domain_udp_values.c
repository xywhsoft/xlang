#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6UdpPacketHandlePolicyV1Get())&&
    xl6OwnershipCatalogAddHandleV1(catalog,xl6UdpErrorPacketHandlePolicyV1Get());}
/* Immutable results do not own the producing socket or execution source. */
XL6_OWNERSHIP_QUERY(UdpValues)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtNetUdpErrorPacketOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(UdpValues,load,ownershipDomainUdpValues)
