#include "xl6_execution_tls_internal.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionTlsListenerFamilyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6ExecutionResourceQuiescenceV1Get())&&
        xl6OwnershipCatalogAddTlsListenerEventsV1(catalog,xl6ExecutionTlsListenerDataPolicyV1Get());
}
XL6_OWNERSHIP_QUERY(ExecutionTlsListener)
{
    (void)completion;
    *adapter=xl6ExecutionOwnershipAdapterV1(ref,policies->ExecutionFamily,policies->ExecutionFamilyCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(ExecutionTlsListener,load,ownershipDomainExecutionTlsListener)
