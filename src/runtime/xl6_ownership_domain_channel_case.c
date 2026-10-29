#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_channel_case.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelCaseHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelSelectionHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ChannelCaseQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ChannelSelectionQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ChannelCase)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xl6ChannelCaseOwnershipAdapterV1(ref); return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ChannelCase,load,ownershipDomainChannelCase)
