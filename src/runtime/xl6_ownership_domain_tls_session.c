#include "xl6_tls_session_internal.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6TlsSessionHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6TlsSessionQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsSession)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xl6TlsSessionOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsSession,load,ownershipDomainTlsSession)
