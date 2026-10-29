/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_dynamic_fields.h"

static bool ownershipLoadDynamicFields(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddHandleV1(catalog,xl6DynamicFieldsHandlePolicyV1Get()); }
XL6_OWNERSHIP_QUERY(DynamicFields)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtDynamicFieldsOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainDynamicFieldsV1Get(void)
{
    static const XL6_OwnershipDomainV1 domain={sizeof(domain),ownershipLoadDynamicFields,
        ownershipDomainDynamicFields,xl6DynamicFieldsReadAdmitsV1};return &domain;
}
