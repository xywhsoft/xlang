#include "src/runtime/xl6_crc32_internal.h"
int64_t xl6Crc32StringV1(uint64_t previous, xstrview text, int64_t size)
{
    if (!xl6Crc32Previous(previous) || !xl6TextSelectRaw(&text, size)) return 0;
    return xrtCrc32Update((uint32_t)previous, text.Data, text.Size);
}
