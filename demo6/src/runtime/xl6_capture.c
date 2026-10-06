#define XRT_MODULE_MEMORY
#define XRT_MODULE_VALUE
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "xl6_capture.h"
#include "xl6_closure.h"
#include "xl6_cell.h"
#include "xl6_value_iterator.h"
#include "xl6_generator.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <stdlib.h>

static bool layout_valid(const XL6_CaptureLayoutV1* layout, size_t bytes)
{
    if (!layout || layout->size != sizeof(*layout) || !bytes || layout->bytes != bytes ||
        layout->count > SIZE_MAX / sizeof(*layout->slots) ||
        (layout->count ? !layout->slots : layout->slots != NULL)) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_CaptureSlot* slot = &layout->slots[i];
        if (slot->kind < XL6_CAPTURE_TRIVIAL || slot->kind > XL6_CAPTURE_GENERATOR || !slot->bytes ||
            slot->offset > bytes || slot->bytes > bytes - slot->offset ||
            (slot->kind == XL6_CAPTURE_STRING && slot->bytes != sizeof(xstrview)) ||
            (slot->kind != XL6_CAPTURE_TRIVIAL && slot->kind != XL6_CAPTURE_STRING &&
             slot->bytes != sizeof(void*))) return false;
        for (size_t j = 0; j < i; ++j) {
            const XL6_CaptureSlot* prior = &layout->slots[j];
            if (slot->offset < prior->offset + prior->bytes && prior->offset < slot->offset + slot->bytes)
                return false;
        }
    }
    return true;
}
static bool capture_trace(const XL6_CaptureSlot* slot, const void* data,
    XL6_ModuleOwnershipVisit visit, void* context)
{
        xrtownershipref ref;
        switch (slot->kind) {
#define CAPTURE_VIEW(KIND, TYPE, VIEW) \
        case KIND: { TYPE item; memcpy(&item, (const char*)data + slot->offset, sizeof(item)); ref = VIEW(item); break; }
        CAPTURE_VIEW(XL6_CAPTURE_XVALUE, xvalue*, xrtValueOwnership)
        CAPTURE_VIEW(XL6_CAPTURE_FUTURE, xfuture*, xrtFutureOwnership)
        CAPTURE_VIEW(XL6_CAPTURE_CLOSURE, XL6_Closure*, xl6ClosureRuntimeV1Get()->Ownership)
        CAPTURE_VIEW(XL6_CAPTURE_ERROR, xerror*, xrtErrorOwnership)
        CAPTURE_VIEW(XL6_CAPTURE_CELL, XL6_Cell*, xl6CellRuntimeV1Get()->Ownership)
        CAPTURE_VIEW(XL6_CAPTURE_VALUE_ITERATOR, XL6_ValueIterator*, xl6ValueIteratorRuntimeV2Get()->Ownership)
        CAPTURE_VIEW(XL6_CAPTURE_GENERATOR, XL6_Generator*, xl6GeneratorRuntimeV1Get()->Ownership)
#undef CAPTURE_VIEW
        case XL6_CAPTURE_TRIVIAL: case XL6_CAPTURE_STRING: return true;
        default: return false;
        }
        return visit(ref, context);
}
static bool layout_trace(const XL6_CaptureLayoutV1* layout, const void* data,
    XL6_ModuleOwnershipVisit visit, void* context)
{
    if (!data || !visit || !layout_valid(layout, layout ? layout->bytes : 0)) return false;
    for (size_t i = 0; i < layout->count; ++i)
        if (!capture_trace(&layout->slots[i], data, visit, context)) return false;
    return true;
}
static void slot_take(void* target, void* output, const void* empty, size_t bytes, unsigned char* initialized)
{
    xrtownershipscope mutation = {0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    memcpy(output, target, bytes); memcpy(target, empty, bytes);
    if (initialized) *initialized = 0;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
}
static void capture_cleanup(const XL6_CaptureSlot* slot, void* data, unsigned char* initialized)
{
        if (initialized && !*initialized) return;
        void* target = (char*)data + slot->offset;
        switch (slot->kind) {
        case XL6_CAPTURE_STRING: {
            xstrview item, empty = {0};
            slot_take(target, &item, &empty, sizeof(item), initialized);
            xrtFree((void*)item.Data);
            break;
        }
#define CAPTURE_DROP(KIND, TYPE, DROP) \
        case KIND: { TYPE item; TYPE empty = NULL; slot_take(target, &item, &empty, sizeof(item), initialized); DROP(item); break; }
        CAPTURE_DROP(XL6_CAPTURE_XVALUE, xvalue*, xrtValueRelease)
        CAPTURE_DROP(XL6_CAPTURE_FUTURE, xfuture*, xrtFutureDestroy)
        CAPTURE_DROP(XL6_CAPTURE_CLOSURE, XL6_Closure*, xl6ClosureRuntimeV1Get()->Release)
        CAPTURE_DROP(XL6_CAPTURE_ERROR, xerror*, xrtErrorFree)
        CAPTURE_DROP(XL6_CAPTURE_CELL, XL6_Cell*, xl6CellRuntimeV1Get()->Release)
        CAPTURE_DROP(XL6_CAPTURE_VALUE_ITERATOR, XL6_ValueIterator*, xl6ValueIteratorRuntimeV2Get()->Destroy)
        CAPTURE_DROP(XL6_CAPTURE_GENERATOR, XL6_Generator*, xl6GeneratorRuntimeV1Get()->Destroy)
#undef CAPTURE_DROP
        case XL6_CAPTURE_TRIVIAL: return;
        default: abort();
        }
}
static void layout_cleanup(const XL6_CaptureLayoutV1* layout, void* data)
{
    xerror* primary;
    if (!data || !layout_valid(layout, layout ? layout->bytes : 0) ||
        xrtMemRangesOverlap(data, layout->bytes, layout, sizeof(*layout)) ||
        (layout->count && xrtMemRangesOverlap(data, layout->bytes,
            layout->slots, layout->count * sizeof(*layout->slots)))) abort();
    primary = xrtTakeError();
    for (size_t i = layout->count; i != 0; --i) {
        capture_cleanup(&layout->slots[i - 1], data, NULL);
        if (!primary) primary = xrtTakeError(); else xrtClearError();
    }
    xrtSetErrorTake(primary);
}
const XL6_CaptureRuntimeV1* xl6CaptureRuntimeV1Get(void)
{
    static const XL6_CaptureRuntimeV1 api = {layout_valid, layout_trace, layout_cleanup};
    return &api;
}

static bool activation_valid(const XL6_ActivationCaptureLayoutV1* layout, size_t bytes)
{
    size_t owning = 0;
    if (!layout || layout->size != sizeof(*layout) || !bytes || layout->bytes != bytes ||
        layout->count > SIZE_MAX / sizeof(*layout->slots) ||
        (layout->count ? !layout->slots : layout->slots != NULL) ||
        layout->cleanupCount > layout->count ||
        (layout->cleanupCount ? !layout->cleanup : layout->cleanup != NULL)) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_ActivationCaptureSlot* slot = &layout->slots[i];
        XL6_CaptureLayoutV1 single = {sizeof(single), bytes, 1, &slot->value};
        if (!layout_valid(&single, bytes)) return false;
        if (slot->value.kind == XL6_CAPTURE_TRIVIAL) {
            if (slot->initialized != SIZE_MAX) return false;
        } else {
            if (slot->initialized >= bytes) return false;
            ++owning;
        }
        for (size_t j = 0; j < layout->count; ++j) {
            const XL6_ActivationCaptureSlot* other = &layout->slots[j];
            /* Bounds are checked before adding, including not-yet-visited slots. */
            if (other->value.offset > bytes || other->value.bytes > bytes - other->value.offset) return false;
            if (slot->initialized != SIZE_MAX &&
                ((slot->initialized >= other->value.offset && slot->initialized - other->value.offset < other->value.bytes) ||
                 (i != j && slot->initialized == other->initialized))) return false;
            if (i != j && slot->value.offset < other->value.offset + other->value.bytes &&
                other->value.offset < slot->value.offset + slot->value.bytes) return false;
        }
    }
    if (owning != layout->cleanupCount) return false;
    for (size_t i = 0; i < layout->cleanupCount; ++i) {
        size_t index = layout->cleanup[i];
        if (index >= layout->count || layout->slots[index].value.kind == XL6_CAPTURE_TRIVIAL) return false;
        for (size_t j = 0; j < i; ++j) if (layout->cleanup[j] == index) return false;
    }
    return true;
}
static bool activation_data_valid(const XL6_ActivationCaptureLayoutV1* layout, const void* data)
{
    if (!data || !activation_valid(layout, layout ? layout->bytes : 0) ||
        xrtMemRangesOverlap(data, layout->bytes, layout, sizeof(*layout)) ||
        (layout->count && xrtMemRangesOverlap(data, layout->bytes, layout->slots, layout->count * sizeof(*layout->slots))) ||
        (layout->cleanupCount && xrtMemRangesOverlap(data, layout->bytes, layout->cleanup, layout->cleanupCount * sizeof(*layout->cleanup)))) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        size_t flag = layout->slots[i].initialized;
        if (flag != SIZE_MAX && ((const unsigned char*)data)[flag] > 1) return false;
    }
    return true;
}
static bool activation_trace(const XL6_ActivationCaptureLayoutV1* layout, const void* data,
    XL6_ModuleOwnershipVisit visit, void* context)
{
    if (!visit || !activation_data_valid(layout, data)) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        const XL6_ActivationCaptureSlot* slot = &layout->slots[i];
        if (slot->initialized != SIZE_MAX && ((const unsigned char*)data)[slot->initialized] &&
            !capture_trace(&slot->value, data, visit, context)) return false;
    }
    return true;
}
static xl6_error_result activation_cleanup(const XL6_ActivationCaptureLayoutV1* layout, void* data, xl6_error_result outcome)
{
    xl6_error_scope cleanup = {0};
    if (!activation_data_valid(layout, data) || !xl6ErrorScopeBegin(&cleanup)) abort();
    xrtSetErrorTake(outcome.error);
    if (!outcome.ok) xl6ErrorScopeCaptureFailure(&cleanup);
    for (size_t i = 0; i < layout->cleanupCount; ++i) {
        const XL6_ActivationCaptureSlot* slot = &layout->slots[layout->cleanup[i]];
        capture_cleanup(&slot->value, data, (unsigned char*)data + slot->initialized);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
    }
    return xl6ErrorScopeEnd(&cleanup);
}
const XL6_ActivationCaptureRuntimeV1* xl6ActivationCaptureRuntimeV1Get(void)
{
    static const XL6_ActivationCaptureRuntimeV1 api = {activation_valid, activation_trace, activation_cleanup};
    return &api;
}
