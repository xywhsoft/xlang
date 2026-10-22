#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_engine.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    if(!xl6OwnershipCatalogAddNetTaskV1(catalog,xl6NetEngineWorkPolicyV1Get()) ||
       !xl6OwnershipCatalogAddNetTimerV1(catalog,xl6NetEngineTimerPolicyV1Get()) ||
       !xl6OwnershipCatalogAddNetWorkerExitV1(catalog,xl6NetEngineExitPolicyV1Get()))return false;
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetEngineOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetEngineNative)
{
    (void)completion;
    /* A selected family registers exact permanent capabilities during Load.
     * Engine neither imports every producer nor guesses a policy from shape.
     * The sealed, typed lists include the built-in detached-work family. */
    *adapter=xrtNetEngineOwnershipAdapterV3(ref,
        policies->NetTask,policies->NetTaskCount,
        policies->NetTimer,policies->NetTimerCount,
        policies->NetWorkerExit,policies->NetWorkerExitCount,
        policies->NetRuntime,policies->NetRuntimeCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetEngineNative,load,ownershipDomainNetEngineNative)
