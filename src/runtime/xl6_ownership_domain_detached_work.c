/* Shared checked work nodes, independent of their native producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_detached_work.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6DetachedWorkQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(DetachedWork)
{
    (void)policies;(void)completion;
    *adapter=xl6DetachedWorkOwnershipAdapterV1(ref,preparation);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(DetachedWork,load,ownershipDomainDetachedWork)
