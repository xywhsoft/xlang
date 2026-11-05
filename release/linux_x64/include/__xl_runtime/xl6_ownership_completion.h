#ifndef XL6_OWNERSHIP_COMPLETION_H
#define XL6_OWNERSHIP_COMPLETION_H
#include "src/common/xl6_xrt.h"

/* Size is read before any version-specific tail. A V1 descriptor may end at
 * a page boundary; unknown sizes are not permission to inspect its tail. */
static inline const xrtownershipfinishv2* xl6OwnershipCompletionV2Get(
    const xrtownershipfinishv1* completion)
{
    if(!completion || completion->size!=sizeof(xrtownershipfinishv2))return NULL;
    const xrtownershipfinishv2* v2=(const xrtownershipfinishv2*)completion;
    return v2->AbiTag==XRT_OWNERSHIP_FINISH_V2_TAG && v2->DropChecked && v2->DropReady ? v2 : NULL;
}
static inline bool xl6OwnershipCompletionValid(const xrtownershipfinishv1* completion,
    const xrtownershipadapterv1* adapter)
{
    if(!completion)return true;
    if(completion->Adapter!=adapter || !completion->Step)return false;
    return completion->size==sizeof(*completion) || xl6OwnershipCompletionV2Get(completion)!=NULL;
}
#endif
