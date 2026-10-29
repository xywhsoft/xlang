/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_closure.h"

XL6_OWNERSHIP_QUERY(Closure)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xl6ClosureOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtValueCallableOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(Closure,NULL,ownershipDomainClosure)
