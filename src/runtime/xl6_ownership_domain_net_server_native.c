#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Server, real counted child bindings, and their actual Listener ownership
 * contexts. Accept Future activation is an independent selected domain. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetServerRuntimeOwnershipV1Get()) &&
        xl6OwnershipCatalogAddListenerEventsV1(catalog,xrtNetServerListenerEventPolicyV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetServerOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetServerBindingOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetServerNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetServerOwnershipAdapterV1(ref,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetServerNative,load,ownershipDomainNetServerNative)
