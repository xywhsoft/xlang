#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_engine.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetEngineOwnershipQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(NetEngineNative)
{
    (void)completion;
    const xnettaskownershipv1* tasks[]={xl6NetEngineWorkPolicyV1Get()};
    const xnettimerownershipv1* timers[]={xl6NetEngineTimerPolicyV1Get()};
    const xnetengineworkerexitownershipv1* exits[]={xl6NetEngineExitPolicyV1Get()};
    /* Only separately selected transport domains populate these exact
     * capabilities. Engine alone neither imports TCP nor admits its slots. */
    *adapter=xrtNetEngineOwnershipAdapterV3(ref,tasks,1,timers,1,exits,1,
        policies->NetRuntime,policies->NetRuntimeCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetEngineNative,load,ownershipDomainNetEngineNative)
