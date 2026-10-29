#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_deadline_service.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6DeadlineServiceQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(DeadlineService)
{
    (void)policies;(void)completion;
    *adapter=xl6DeadlineServiceOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(DeadlineService,load,ownershipDomainDeadlineService)
