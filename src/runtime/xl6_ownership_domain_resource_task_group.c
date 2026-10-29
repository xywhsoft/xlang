/* Exact resident TaskGroup policy population and independent physical nodes. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_ownership_cancel.h"
#include "xl6_native_handle.h"
#include "src/api/xl_task_group_bridge.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6TaskGroupQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskGroupOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskGroupItemOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtFutureOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtCancelOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtCancelWatchOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6TaskGroupHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6TaskGroupSnapshotHandlePolicyV1Get()) &&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtTaskGroupProducerPolicyV1Get()) &&
        xl6OwnershipCatalogAddWatchV1(catalog,xrtTaskGroupWatchPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtTaskGroupCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xl6TaskGroupParentPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(ResourceTaskGroup)
{
    *adapter=xl6TaskGroupOwnershipAdapterV2(ref,preparation,completion);
    if(!*adapter)*adapter=xrtTaskGroupOwnershipAdapterV2(ref,preparation,completion);
    /* A synchronous TaskGroup still owns native endpoints/tokens, even when
     * no language Future payload component is selected. Use the SAME exact
     * foundation identities as other domains, never conflicting adapters. */
    const xfutureownershipadmissionv1 admission={sizeof(admission),
        policies->Payload,policies->PayloadCount,policies->Producer,policies->ProducerCount,
        policies->Watch,policies->WatchCount,policies->ProjectedWatch,policies->ProjectedWatchCount,
        policies->CancelWatch,policies->CancelWatchCount};
    if(!*adapter)*adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation);
    if(!*adapter)(void)xl6OwnershipResolveCancelV1(ref,policies,adapter,completion);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ResourceTaskGroup,load,ownershipDomainResourceTaskGroup)
