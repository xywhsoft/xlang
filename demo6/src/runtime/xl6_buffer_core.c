#include "src/runtime/xl6_buffer_internal.h"

void xl6BufferDestroyV1(void* raw)
{
    /* Cleanup must not replace an error already being propagated, including
     * an allocator's reentrant diagnostics. Payloads come from trusted XRT. */
    xerror* failure = xrtTakeError();
    xrtBufferDestroy(raw); xrtClearError(); xrtSetErrorTake(failure);
}
void* xl6BufferCreateV1(void)
{
    xbuffer* result = xrtBufferCreate();
    xl6BufferAllocation(result != NULL); return result;
}
void* xl6BufferCreateReserveV1(int64_t capacity)
{
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    xbuffer* result = xl6BufferCreateV1();
    if (result && !xl6BufferAllocation(xrtBufferReserve(result, (size_t)capacity))) {
        xl6BufferDestroyV1(result); return NULL;
    }
    return result;
}
void* xl6BufferAdoptV1(void* raw)
{ return xl6BufferChecked(raw); } /* Explicit C interop: success transfers ownership. */
int64_t xl6BufferLengthV1(void* raw)
{ xbuffer* buffer = xl6BufferChecked(raw); return buffer ? (int64_t)buffer->Size : 0; }
int64_t xl6BufferCapacityV1(void* raw)
{ xbuffer* buffer = xl6BufferChecked(raw); return buffer ? (int64_t)buffer->Capacity : 0; }
bool xl6BufferReserveV1(void* raw, int64_t capacity)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xl6BufferAllocation(xrtBufferReserve(buffer, (size_t)capacity));
}
bool xl6BufferClearV1(void* raw)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    xrtBufferClear(buffer); return true;
}
static bool rawInput(void* raw, int64_t size, xbytesview* view)
{
    if (size < 0 || (uint64_t)size > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    *view = (xbytesview){raw, (size_t)size};
    return xl6BufferSelect(view, size);
}
bool xl6BufferAppendRawV1(void* raw, void* data, int64_t size)
{
    xbuffer* buffer = xl6BufferChecked(raw); xbytesview view;
    if (!buffer || !rawInput(data, size, &view)) return false;
    if (view.Size > (size_t)INT64_MAX - buffer->Size) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xl6BufferAllocation(xrtBufferAppend(buffer, view));
}
bool xl6BufferInsertRawV1(void* raw, int64_t offset, void* data, int64_t size)
{
    xbuffer* buffer = xl6BufferChecked(raw); xbytesview view;
    if (!buffer || !xl6BufferOffset(buffer, offset) || !rawInput(data, size, &view)) return false;
    if (view.Size > (size_t)INT64_MAX - buffer->Size) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xl6BufferAllocation(xrtBufferInsert(buffer, (size_t)offset, view));
}
bool xl6BufferAppendByteV1(void* raw, int64_t value)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (value < 0 || value > 255) { xrtSetErrorKind(XERR_VALUE); return false; }
    if (buffer->Size == INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    return xl6BufferAllocation(xrtBufferAppendByte(buffer, (uint8_t)value));
}
int64_t xl6BufferByteAtV1(void* raw, int64_t index)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return 0;
    if (index < 0 || (uint64_t)index >= buffer->Size) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return buffer->Data[index];
}
bool xl6BufferSetByteV1(void* raw, int64_t index, int64_t value)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (value < 0 || value > 255) { xrtSetErrorKind(XERR_VALUE); return false; }
    if (index < 0 || (uint64_t)index >= buffer->Size) { xrtSetErrorKind(XERR_RANGE); return false; }
    buffer->Data[index] = (uint8_t)value; return true;
}
static bool range(const xbuffer* buffer, int64_t offset, int64_t* count)
{
    if (!xl6BufferOffset(buffer, offset) || *count < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    size_t available = buffer->Size - (size_t)offset;
    if (*count == -1) *count = (int64_t)available;
    if ((uint64_t)*count > available) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
bool xl6BufferCopyFromV1(void* targetRaw, int64_t offset, void* sourceRaw, int64_t sourceOffset, int64_t count)
{
    xbuffer* target = xl6BufferChecked(targetRaw);
    xbuffer* source = xl6BufferChecked(sourceRaw);
    if (!target || !source || !range(source, sourceOffset, &count)) return false;
    if (offset < 0 || (uint64_t)offset > SIZE_MAX || (uint64_t)count > (uint64_t)INT64_MAX - (uint64_t)offset) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    xbytesview view = {count ? source->Data + sourceOffset : NULL, (size_t)count};
    return xl6BufferAllocation(xrtBufferWrite(target, (size_t)offset, view));
}
bool xl6BufferCopyWithinV1(void* raw, int64_t offset, int64_t sourceOffset, int64_t count)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (count < 0) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!buffer || !range(buffer, sourceOffset, &count) || !range(buffer, offset, &count)) return false;
    if (count) memmove(buffer->Data + offset, buffer->Data + sourceOffset, (size_t)count);
    return true;
}
bool xl6BufferFillV1(void* raw, int64_t value, int64_t offset, int64_t count)
{
    xbuffer* buffer = xl6BufferChecked(raw);
    if (!buffer) return false;
    if (value < 0 || value > 255) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!range(buffer, offset, &count)) return false;
    if (count) memset(buffer->Data + offset, (uint8_t)value, (size_t)count);
    return true;
}
void* xl6BufferDataV1(void* raw)
{ xbuffer* buffer = xl6BufferChecked(raw); return buffer ? buffer->Data : NULL; }
void* xl6BufferHandleV1(void* raw)
{ return xl6BufferChecked(raw); }
