/* Genuine selected supplier implementations; no test slot-map algorithm. */
#define XRUNTIME_MODULE_TYPED_DICT
#define XRUNTIME_MODULE_TYPED_VALUE
#define XRUNTIME_MODULE_TYPED_ARRAY
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#define XRT_MODULE_SLOT_MAP
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_ONCE
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include <assert.h>

/* White-box generation-exhaustion seam: actual generated supplier record.
 * The test does not claim to execute UINT32_MAX remove/insert cycles. */
xslot xl6TestSlotMapGenerationLimit(xslotmap* map,xslot slot)
{
    uint32 index=xrtSlotIndex(slot); assert(index<map->Storage.Count);
    xslotentry* entry=(xslotentry*)map->Storage.Data+index; assert(entry->Value);
    entry->Generation=UINT32_MAX;
    return ((xslot)UINT32_MAX<<32u)|((xslot)index+1u);
}
