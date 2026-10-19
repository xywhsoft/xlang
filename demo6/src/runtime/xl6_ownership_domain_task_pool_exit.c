#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_task_pool_exit.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddTaskPoolExitV1(catalog,xl6TaskPoolWorkerExitPolicyV1Get());}
const XL6_OwnershipDomainV1* xl6OwnershipDomainTaskPoolExitV1Get(void)
{static const XL6_OwnershipDomainV1 domain={sizeof(domain),load,NULL,NULL};return &domain;}
