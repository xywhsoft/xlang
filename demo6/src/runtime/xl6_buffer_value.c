#include "src/runtime/xl6_buffer_internal.h"

static void* bufferCopy(xbytesview view)
{
    xbuffer* result = xrtBufferFrom(view);
    xl6BufferAllocation(result != NULL);
    return result;
}
void* xl6BufferFromTextV1(xstrview text, int64_t size)
{
    xbytesview view = {(const unsigned char*)text.Data, text.Size};
    return xl6BufferSelect(&view, size) ? bufferCopy(view) : NULL;
}
void* xl6BufferFromBytesV1(const xvalue* bytes, int64_t size)
{
    xbytesview view;
    return xl6BufferBytesInput(bytes, size, &view) ? bufferCopy(view) : NULL;
}
void* xl6BufferCopyV1(void* source, int64_t size)
{
    xbytesview view;
    return xl6BufferInput(source, size, &view) ? bufferCopy(view) : NULL;
}
bool xl6BufferResizeV1(void* raw, int64_t size, int64_t fill)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (size < 0 || (uint64_t)size > SIZE_MAX || fill < 0 || fill > 255) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    size_t previous = buffer->Size;
    if (!xl6BufferAllocation(xrtBufferResize(buffer, (size_t)size))) return false;
    if ((size_t)size > previous && fill != 0)
        memset(buffer->Data + previous, (unsigned char)fill, (size_t)size - previous);
    return true;
}
bool xl6BufferTrimV1(void* raw)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    return buffer && xl6BufferAllocation(xrtBufferTrim(buffer));
}

/* Input selection precedes XRT's transactional, self-alias-aware operations.
 * Append/insert accept raw byte prefixes of UTF-8 strings; only text output
 * validates that the selected bytes form complete UTF-8 scalars. */
typedef enum BufferOperation { BUFFER_ASSIGN, BUFFER_APPEND, BUFFER_INSERT } BufferOperation;
static bool bufferApply(void* raw, xbytesview view, BufferOperation operation, int64_t offset)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (operation == BUFFER_INSERT && !xl6BufferOffset(buffer, offset)) return false;
    if (operation != BUFFER_ASSIGN && view.Size > (size_t)INT64_MAX - buffer->Size) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    switch (operation) {
    case BUFFER_ASSIGN: return xl6BufferAllocation(xrtBufferAssign(buffer, view));
    case BUFFER_APPEND: return xl6BufferAllocation(xrtBufferAppend(buffer, view));
    case BUFFER_INSERT: return xl6BufferAllocation(xrtBufferInsert(buffer, (size_t)offset, view));
    }
    abort();
}
#define BUFFER_TEXT(Name, Operation) \
    bool Name(void* raw, xstrview text, int64_t size) { \
        xbytesview view = {(const unsigned char*)text.Data, text.Size}; \
        return xl6BufferSelect(&view, size) && bufferApply(raw, view, Operation, 0); \
    }
#define BUFFER_BYTES(Name, Operation) \
    bool Name(void* raw, const xvalue* bytes, int64_t size) { \
        xbytesview view; \
        return xl6BufferBytesInput(bytes, size, &view) && bufferApply(raw, view, Operation, 0); \
    }
#define BUFFER_BUFFER(Name, Operation) \
    bool Name(void* raw, void* source, int64_t size) { \
        xbytesview view; \
        return xl6BufferInput(source, size, &view) && bufferApply(raw, view, Operation, 0); \
    }
BUFFER_TEXT(xl6BufferAssignTextV1, BUFFER_ASSIGN)
BUFFER_BYTES(xl6BufferAssignBytesV1, BUFFER_ASSIGN)
BUFFER_BUFFER(xl6BufferAssignBufferV1, BUFFER_ASSIGN)
BUFFER_TEXT(xl6BufferAppendTextV1, BUFFER_APPEND)
BUFFER_BYTES(xl6BufferAppendBytesV1, BUFFER_APPEND)
BUFFER_BUFFER(xl6BufferAppendBufferV1, BUFFER_APPEND)
#undef BUFFER_TEXT
#undef BUFFER_BYTES
#undef BUFFER_BUFFER
bool xl6BufferInsertTextV1(void* raw, int64_t offset, xstrview text, int64_t size)
{
    xbytesview view = {(const unsigned char*)text.Data, text.Size};
    return xl6BufferSelect(&view, size) && bufferApply(raw, view, BUFFER_INSERT, offset);
}
bool xl6BufferInsertBytesV1(void* raw, int64_t offset, const xvalue* bytes, int64_t size)
{
    xbytesview view;
    return xl6BufferBytesInput(bytes, size, &view) && bufferApply(raw, view, BUFFER_INSERT, offset);
}
bool xl6BufferInsertBufferV1(void* raw, int64_t offset, void* source, int64_t size)
{
    xbytesview view;
    return xl6BufferInput(source, size, &view) && bufferApply(raw, view, BUFFER_INSERT, offset);
}
bool xl6BufferRemoveV1(void* raw, int64_t offset, int64_t count)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer || !xl6BufferOffset(buffer, offset)) return false;
    size_t available = buffer->Size - (size_t)offset;
    if (count < -1 || (count >= 0 && (uint64_t)count > available)) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    return xrtBufferRemove(buffer, (size_t)offset, count < 0 ? available : (size_t)count);
}
xvalue* xl6BufferBytesV1(void* raw, int64_t size)
{
    xbytesview view;
    if (!xl6BufferInput(raw, size, &view)) return NULL;
    xvalue* result = xrtValueBytes(view);
    xl6BufferAllocation(result != NULL);
    return result;
}
xstrview xl6BufferTextV1(void* raw, int64_t size)
{
    xbytesview view;
    if (!xl6BufferInput(raw, size, &view)) return xl6TextFailure;
    xstrview text = {(const char*)view.Data, view.Size};
    if (!xl6TextSelect(&text, -1)) return xl6TextFailure;
    return xl6TextCopy(text);
}
