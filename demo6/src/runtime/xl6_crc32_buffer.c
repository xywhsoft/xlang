#include "src/runtime/xl6_crc32_internal.h"
#include "src/runtime/xl6_buffer_internal.h"
int64_t xl6Crc32BufferV1(uint64_t previous, void* value, int64_t size)
{
    xbytesview view;
    if (!xl6Crc32Previous(previous) || !xl6BufferInput(value, size, &view)) return 0;
    return xrtCrc32Update((uint32_t)previous, view.Data, view.Size);
}
