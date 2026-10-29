/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"

static bool ownershipLoadProcessLive(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddHandleV1(catalog,xl6ProcessHandlePolicyV1Get()); }
XL6_OWNERSHIP_QUERY(ProcessLive)
{
    (void)policies;(void)preparation;
    *adapter=xl6ProcessLiveOwnershipAdapterV1(ref);
    if(*adapter)*completion=xl6ProcessLiveOwnershipFinishV1Get();
    else *adapter=xrtProcessOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ProcessLive,ownershipLoadProcessLive,ownershipDomainProcessLive)
