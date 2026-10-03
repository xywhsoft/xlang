#include "src/runtime/xl6_buffer_internal.h"

int64_t xl6BufferCompareV1(void* left, void* right, int64_t size, int64_t otherSize)
{
    xbytesview a, b;
    if (!xl6BufferInput(left, size, &a) || !xl6BufferInput(right, otherSize, &b)) return 0;
    return xrtStrCompare((xstrview){(const char*)a.Data,a.Size}, (xstrview){(const char*)b.Data,b.Size});
}
int64_t xl6BufferIndexOfV1(void* raw, void* needle, int64_t start, int64_t size, int64_t needleSize)
{
    xbytesview a, b;
    if (!xl6BufferInput(raw, size, &a) || !xl6BufferInput(needle, needleSize, &b)) return -1;
    if (start < 0) { xrtSetErrorKind(XERR_RANGE); return -1; }
    if ((uint64_t)start > a.Size) return -1;
    size_t found = xrtStrFind((xstrview){(const char*)a.Data,a.Size}, (xstrview){(const char*)b.Data,b.Size}, (size_t)start);
    return found == XRT_NPOS ? -1 : (int64_t)found;
}
