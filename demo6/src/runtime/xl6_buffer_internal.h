#ifndef XL6_BUFFER_INTERNAL_H
#define XL6_BUFFER_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"
#include <stdlib.h>
#include <string.h>

/* Buffer remains the actual XRT byte resource. IO/hash/random adapters can
 * consume this native payload without a second layout or wrapper allocation.
 * This validates descriptors, not arbitrary pointers supplied through C FFI. */
static inline xbuffer* xl6BufferChecked(void* raw)
{
    xbuffer* buffer = raw;
    if (!buffer || buffer->Size > buffer->Capacity ||
        buffer->Capacity > INT64_MAX || (buffer->Capacity && !buffer->Data)) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return NULL;
    }
    return buffer;
}
static inline bool xl6BufferSelect(xbytesview* view, int64_t size)
{
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (view->Size && !view->Data) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (size >= 0 && (uint64_t)size < view->Size) view->Size = (size_t)size;
    if (view->Size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
static inline bool xl6BufferAllocation(bool ok)
{
    if (!ok && xrtErrorKind(xrtGetError()) == XERR_NONE)
        xrtSetErrorKind(XERR_MEMORY);
    return ok;
}
static inline bool xl6BufferBytesInput(const xvalue* value, int64_t size, xbytesview* view)
{
    if (!value || !xrtValueGetBytes(value, view)) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    return xl6BufferSelect(view, size);
}
static inline bool xl6BufferInput(void* raw, int64_t size, xbytesview* view)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    *view = xrtBufferView(buffer);
    return xl6BufferSelect(view, size);
}
static inline bool xl6BufferOffset(const xbuffer* buffer, int64_t offset)
{
    if (offset < 0 || (uint64_t)offset > buffer->Size) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    return true;
}
#endif
