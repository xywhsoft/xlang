#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
#include "xl6_tls_verifier_context.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6TlsVerifierHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddTlsVerifierContextV1(catalog,xl6TlsVerifierContextNativePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTlsVerifierOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6TlsVerifierContextQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(TlsVerifier)
{
    (void)completion;
    *adapter=xl6TlsVerifierContextOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtTlsVerifierOwnershipAdapterV1(ref,
        policies->TlsVerifierContext,policies->TlsVerifierContextCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(TlsVerifier,load,ownershipDomainTlsVerifier)
