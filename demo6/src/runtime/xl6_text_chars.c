#include "src/runtime/xl6_text_internal.h"

xvalue* xl6TextCharsV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return NULL;
    xvalue* result = xrtValueArray();
    if (!result) return NULL;
    size_t index = 0;
    while (index < text.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){text.Data + index, text.Size - index}, &scalar, &read) != XUTF_OK) goto fail;
        xvalue* item = xrtValueChar(scalar);
        if (!item) goto fail;
        if (!xrtValueArrayAppendNew(result, item)) goto fail;
        index += read;
    }
    return result;
fail:
    xrtValueRelease(result);
    return NULL;
}
