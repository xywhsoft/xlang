/* Exact resident capability; selected independently of its producer. */
#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"

XL6_OWNERSHIP_QUERY(NativeFuture)
{
    (void)completion;
    const xfutureownershipadmissionv1 admission={sizeof(admission),
        policies->Payload,policies->PayloadCount,policies->Producer,policies->ProducerCount,
        policies->Watch,policies->WatchCount,policies->ProjectedWatch,policies->ProjectedWatchCount,
        policies->CancelWatch,policies->CancelWatchCount};
    *adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation);
    if(!*adapter)*adapter=xrtCancelWatchOwnershipAdapterV1(ref,policies->CancelWatch,policies->CancelWatchCount,preparation);
    if(!*adapter)*adapter=xrtCancelOwnershipAdapterV2(ref,policies->CancelWatch,policies->CancelWatchCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NativeFuture,NULL,ownershipDomainNativeFuture)
