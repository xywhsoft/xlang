#include "xl6_ws_wait.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* c)
{
    return xl6OwnershipCatalogAddNetTaskV1(c,xl6WsWaitPostPolicyV2Get())&&
        xl6OwnershipCatalogAddNetTimerV1(c,xl6WsWaitTimerPolicyV2Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(c,xl6WsWaitCancelPolicyV2Get());
}
XL6_OWNERSHIP_DOMAIN(WsWait,load,NULL)
