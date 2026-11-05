#ifndef XL6_OWNERSHIP_FINALIZATION_H
#define XL6_OWNERSHIP_FINALIZATION_H
#include "src/common/xl6_xrt.h"
/* Only exact immutable typed metadata is admitted. No larger-layout guessing,
 * semantic/user call, allocator or diagnostic mutation in this frozen query. */
static inline bool xl6OwnershipFinalizationValidV1(
    const xrtownershipfinalizationv1* value,const xrtownershipadapterv1* adapter)
{
    return !value || (value->size==sizeof(*value) &&
        value->AbiTag==XRT_OWNERSHIP_FINALIZATION_V1_TAG &&
        value->Adapter && (!adapter || value->Adapter==adapter) && value->CanStep && value->Step);
}
#endif
