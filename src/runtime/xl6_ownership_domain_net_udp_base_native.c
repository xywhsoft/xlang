#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetUdpRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetUdpOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetUdpBaseNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetUdpOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetUdpPacketOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtNetUdpErrorPacketOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetUdpBaseNative,load,ownershipDomainNetUdpBaseNative)
