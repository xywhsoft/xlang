/* Compile only against the staged declaration SDK, not source-tree siblings. */
#include <__xl_runtime/xl6_ownership_catalog.h>
#include <__xl_runtime/xl6_ownership_domains.h>
int ownership_catalog_declaration_probe(void)
{
    return sizeof(XL6_OwnershipDomainV1)>0 && sizeof(XL6_OwnershipPoliciesV1)>0;
}
