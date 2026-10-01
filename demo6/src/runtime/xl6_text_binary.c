#include "src/runtime/xl6_text_internal.h"

int64_t xl6TextByteLengthV1(xstrview text, int64_t size)
{ return xl6TextSelectRaw(&text, size) ? (int64_t)text.Size : 0; }
int64_t xl6TextByteAtV1(xstrview text, int64_t index, int64_t size)
{
    if (!xl6TextSelectRaw(&text, size)) return 0;
    if (index < 0 && index >= -(int64_t)text.Size) index += (int64_t)text.Size;
    if (index < 0 || (uint64_t)index >= text.Size) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (uint8_t)text.Data[index];
}
xvalue* xl6TextBytesV1(xstrview text, int64_t size)
{
    if (!xl6TextSelectRaw(&text, size)) return NULL;
    return xrtValueBytes((xbytesview){(const unsigned char*)text.Data, text.Size});
}
