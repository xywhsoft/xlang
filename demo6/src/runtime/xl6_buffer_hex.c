#include "src/runtime/xl6_buffer_internal.h"

static bool hexFlags(int64_t flags)
{
    if (flags < 0 || (uint64_t)flags > UINT32_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return true; /* Direction-specific flags and diagnostics belong to XRT. */
}
void* xl6BufferFromHexV1(xstrview text, int64_t flags, int64_t size)
{
    if (!hexFlags(flags) || !xl6TextSelectRaw(&text, size)) return NULL;
    xbuffer* result = xrtBufferFromHex(text, (uint32_t)flags);
    xl6BufferAllocation(result != NULL);
    return result;
}
xstrview xl6BufferHexV1(void* raw, int64_t flags, int64_t size)
{
    xbytesview input;
    size_t output = 0;
    if (!hexFlags(flags) || !xl6BufferInput(raw, size, &input) ||
        !xrtHexEncode(input.Data, input.Size, NULL, 0, &output, (uint32_t)flags))
        return xl6TextFailure;
    if (output > INT64_MAX || output == SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    char* text = xrtMalloc(output + 1);
    if (!text) return xl6TextOwn(NULL, 0);
    if (!xrtHexEncode(input.Data, input.Size, text, output + 1, &output, (uint32_t)flags)) {
        xerror* failure = xrtTakeError();
        xrtFree(text); xrtClearError(); xrtSetErrorTake(failure);
        return xl6TextFailure;
    }
    return (xstrview){text, output};
}
