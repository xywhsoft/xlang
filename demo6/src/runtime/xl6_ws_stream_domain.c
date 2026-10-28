#include "xl6_ws_stream_internal.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* c)
{
    return xl6OwnershipCatalogAddHandleV1(c,xl6WsStreamHandlePolicyV2Get())&&
        xl6OwnershipCatalogAddExecutionFamilyV1(c,xl6WsExecutionFamilyV2Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(c,xl6ExecutionResourceQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(c,xrtWsStreamOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddNetRuntimeV1(c,xrtWsStreamRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddStreamEventsV1(c,xrtWsStreamTcpEventsOwnershipV1Get());
}
XL6_OWNERSHIP_QUERY(WsStream)
{
    (void)completion;(void)policies;
    const xwsstreamownershipv1* allowed[]={xl6WsStreamDataPolicyV2Get()};
    *adapter=xrtWsStreamOwnershipAdapterV1(ref,allowed,1,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(WsStream,load,ownershipDomainWsStream)
