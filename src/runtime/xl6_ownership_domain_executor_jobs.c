#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_executor_jobs.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddExecutorV1(catalog,xl6ExecutorClosureDataPolicyV1Get())&&
        xl6OwnershipCatalogAddExecutorExitV1(catalog,xl6ExecutorWorkerExitPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutorClosureQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(ExecutorJobs)
{
    (void)policies;(void)completion;
    *adapter=xl6ExecutorJobsOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutorJobs,load,ownershipDomainExecutorJobs)
