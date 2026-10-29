#include "src/runtime/xl6_codec_internal.h"

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
    return xl6BufferInput(raw,size,&input)?xl6CodecHexViewV1(input,flags):xl6TextFailure;
}
