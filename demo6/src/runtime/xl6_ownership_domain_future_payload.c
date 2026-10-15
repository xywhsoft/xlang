/* Exact resident capability; selected independently of its producer. */
#include "release/include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_future_payload.h"

static bool ownershipLoadFuturePayload(XL6_OwnershipCatalog* catalog)
{
    for(unsigned kind=XL6_FUTURE_PAYLOAD_VALUE;kind<XL6_FUTURE_PAYLOAD_COUNT;++kind)
        if(!xl6OwnershipCatalogAddPayloadV1(catalog,xl6PayloadPolicyGet((XL6_FuturePayloadKind)kind)))return false;
    return true;
}
XL6_OWNERSHIP_QUERY(FuturePayload)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtValueFutureOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(FuturePayload,ownershipLoadFuturePayload,ownershipDomainFuturePayload)
