/* Exact resident capability; selected independently of its producer. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_ownership_cancel.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtFutureOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtCancelOwnershipQuiescenceV1Get()) &&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtCancelWatchOwnershipQuiescenceV1Get());
}

XL6_OWNERSHIP_QUERY(NativeFuture)
{
    const xfutureownershipadmissionv1 admission={sizeof(admission),
        policies->Payload,policies->PayloadCount,policies->Producer,policies->ProducerCount,
        policies->Watch,policies->WatchCount,policies->ProjectedWatch,policies->ProjectedWatchCount,
        policies->CancelWatch,policies->CancelWatchCount};
    *adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation);
    if(!*adapter)(void)xl6OwnershipResolveCancelV1(ref,policies,adapter,completion);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NativeFuture,load,ownershipDomainNativeFuture)
