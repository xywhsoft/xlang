#ifndef XL6_OWNERSHIP_CANCEL_H
#define XL6_OWNERSHIP_CANCEL_H
#include "include/xruntime_decl.h"
#include "xl6_ownership_catalog.h"

/* One physical cancellation retirement protocol in every resident domain.
 * Clear disables notification but preserves all real slots; the registration
 * owner performs checked Unwatch in Finish. Native completion remains BUSY
 * until the owner consumed that right and all dispatch tails left. Never
 * request cancellation or erase accepted callbacks to make a graph collectible.
 * The caller initializes completion to NULL and invokes this only when no
 * other physical family has matched. Closed policy admission precedes Trace. */
static inline bool xl6OwnershipResolveCancelV1(xrtownershipref ref,
    const XL6_OwnershipPoliciesV1* policies, const xrtownershipadapterv1** adapter,
    const xrtownershipfinishv1** completion)
{
    *adapter = xrtCancelOwnershipAdapterV3(ref, policies->CancelWatch,
        policies->CancelWatchCount, completion);
    if (!*adapter) *adapter = xrtCancelWatchOwnershipAdapterV2(ref,
        policies->CancelWatch, policies->CancelWatchCount, completion);
    return *adapter != NULL;
}
#endif
