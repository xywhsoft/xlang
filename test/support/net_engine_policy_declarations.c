/* Only the copied SDK header tree, not source-tree sibling headers. */
#include <__xl_runtime/xl6_ownership_catalog.h>
typedef bool (*TaskRegistration)(XL6_OwnershipCatalog*,const struct xnettaskownershipv1*);
typedef bool (*TimerRegistration)(XL6_OwnershipCatalog*,const struct xnettimerownershipv1*);
typedef bool (*ExitRegistration)(XL6_OwnershipCatalog*,const struct xnetengineworkerexitownershipv1*);
TaskRegistration taskRegistration(void){return xl6OwnershipCatalogAddNetTaskV1;}
TimerRegistration timerRegistration(void){return xl6OwnershipCatalogAddNetTimerV1;}
ExitRegistration exitRegistration(void){return xl6OwnershipCatalogAddNetWorkerExitV1;}
size_t networkPolicyPopulation(const XL6_OwnershipPoliciesV1* p)
{return p->NetTaskCount+p->NetTimerCount+p->NetWorkerExitCount;}
