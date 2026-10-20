#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_engine.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetEngineOwnershipQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(NetEngineNative)
{
    (void)policies;(void)completion;
    const xnettaskownershipv1* tasks[]={xl6NetEngineWorkPolicyV1Get()};
    const xnettimerownershipv1* timers[]={xl6NetEngineTimerPolicyV1Get()};
    const xnetengineworkerexitownershipv1* exits[]={xl6NetEngineExitPolicyV1Get()};
    *adapter=xrtNetEngineOwnershipAdapterV2(ref,tasks,1,timers,1,exits,1,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetEngineNative,load,ownershipDomainNetEngineNative)
