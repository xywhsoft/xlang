#include "include/xruntime_decl.h"
#include "xl6_ownership_catalog.h"
#include "xl6_ownership_domains.h"
#include "xl6_native_handle.h"
#include "xl6_thread_local.h"
static bool load(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddHandleV1(catalog, xl6ThreadLocalHandlePolicyV1Get()); }
static bool resolve(xrtownershipref ref, const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter, const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)policies; (void)preparation; (void)completion;
    *adapter = xl6ThreadLocalOwnershipAdapterV1(ref); return *adapter != NULL;
}
const XL6_OwnershipDomainV1* xl6OwnershipDomainThreadLocalV1Get(void)
{ static const XL6_OwnershipDomainV1 domain = {sizeof(domain), load, resolve, NULL}; return &domain; }
