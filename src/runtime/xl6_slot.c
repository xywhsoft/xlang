#define XRT_MODULE_MEMORY
#include "src/common/xl6_xrt.h"
#include "xl6_slot.h"
#include "xl6_ownership_scope.h"
#include <string.h>

static void slot_apply_body(const XL6_SlotWrite* writes, size_t count)
{
    size_t bytes;
    if (!count) return;
    if (count>SIZE_MAX/sizeof(*writes)) abort();
    bytes=count*sizeof(*writes);
    if (!writes || !xrtMemRangeValid(writes,bytes)) abort();
    /* Validate the complete descriptor plan before changing any user slot. */
    for (size_t i=0;i<count;++i) {
        const XL6_SlotWrite* w=&writes[i];
        if (!w->target || !w->input || !w->size ||
            !xrtMemRangeValid(w->target,w->size) || !xrtMemRangeValid(w->input,w->size) ||
            xrtMemRangesOverlap(w->target,w->size,writes,bytes) ||
            (w->target!=w->input && xrtMemRangesOverlap(w->target,w->size,w->input,w->size))) abort();
    }
    for (size_t i=0;i<count;++i) {
        const XL6_SlotWrite* w=&writes[i];
        if (w->target!=w->input) memcpy(w->target,w->input,w->size);
    }
}
static void slot_apply(const XL6_SlotWrite* writes, size_t count)
{ XL6_OWNERSHIP_RETURN_VOID(slot_apply_body(writes,count)); }
const XL6_SlotRuntimeV1* xl6SlotRuntimeV1Get(void)
{
    static const XL6_SlotRuntimeV1 api={slot_apply};
    return &api;
}
