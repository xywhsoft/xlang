#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_failures.h"
XL6_OWNERSHIP_QUERY(ExecutionFailures)
{
    (void)policies;(void)completion;*preparation=NULL;
    *adapter=xl6ExecutionFailuresAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionFailures,NULL,ownershipDomainExecutionFailures)
