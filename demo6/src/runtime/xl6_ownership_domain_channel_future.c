#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_channel_future.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddProducerV1(catalog,xl6ChannelFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xl6ChannelFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddQueuedStorageV1(catalog,xl6ChannelFutureStorageV2Get())&&
        xl6OwnershipCatalogAddChannelSubscriptionV1(catalog,xl6ChannelFutureSubscriptionPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtChannelSubscribeOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ChannelFuture)
{
    (void)completion;
    *adapter=xl6ChannelFutureOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,
        policies->ChannelSubscription,policies->ChannelSubscriptionCount);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ChannelFuture,load,ownershipDomainChannelFuture)
