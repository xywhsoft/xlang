/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_continuation_executor.h"

static bool load(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ContinuationExecutorQuiescenceV1Get()); }
XL6_OWNERSHIP_QUERY(Executor)
{
    (void)policies;(void)completion;
    *adapter=xl6ContinuationExecutorOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Executor,load,ownershipDomainExecutor)
