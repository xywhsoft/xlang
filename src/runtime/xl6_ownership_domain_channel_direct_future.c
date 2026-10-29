#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_channel_future_core.h"
#include "xl6_channel_direct_future.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xl6ChannelFutureCoreProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddQueuedStorageV1(catalog,xl6ChannelFutureCoreStorageV2Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xl6ChannelDirectFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddChannelSubscriptionV1(catalog,xl6ChannelDirectFutureSubscriptionPolicyV1Get())&&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelReceiveResultHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ChannelReceiveResultQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ChannelDirectFutureQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtChannelSubscribeOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ChannelDirectFuture)
{
    (void)completion;
    *adapter=xl6ChannelDirectFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,
        policies->ChannelSubscription,policies->ChannelSubscriptionCount);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ChannelDirectFuture,load,ownershipDomainChannelDirectFuture)
