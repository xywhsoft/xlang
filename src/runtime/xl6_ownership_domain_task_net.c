#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_task_net.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddTaskNetV1(catalog,xl6TaskNetDataPolicyV1Get()) &&
        xl6OwnershipCatalogAddNetTaskV1(catalog,xrtTaskNetPostPolicyV1Get()) &&
        xl6OwnershipCatalogAddNetTimerV1(catalog,xrtTaskNetTimerPolicyV1Get()) &&
        xl6OwnershipCatalogAddTaskV1(catalog,xrtTaskNetJobDataPolicyV1Get()) &&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtTaskNetCancelPolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskNetOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TaskNet)
{
    (void)completion;
    *adapter=xrtTaskNetOwnershipAdapterV1(ref,policies->TaskNet,policies->TaskNetCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TaskNet,load,ownershipDomainTaskNet)
