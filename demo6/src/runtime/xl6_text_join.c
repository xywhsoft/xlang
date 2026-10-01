#include "src/runtime/xl6_text_internal.h"

xstrview xl6TextJoinV1(const xvalue* values, xstrview separator, int64_t separatorSize, int64_t itemSize)
{
    if (!xl6TextSelect(&separator, separatorSize)) return xl6TextFailure;
    if (itemSize < -1) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    if (!values || xrtValueType(values) != XVALUE_ARRAY) {
        xrtSetErrorKind(XERR_TYPE); return xl6TextFailure;
    }
    size_t count = xrtValueCount(values);
    if (count > SIZE_MAX / sizeof(xstrview)) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    xstrview* items = count ? xrtMalloc(count * sizeof(*items)) : NULL;
    if (count && !items) { xrtSetErrorKind(XERR_MEMORY); return xl6TextFailure; }
    for (size_t i = 0; i < count; ++i) {
        const xvalue* value = xrtValueArrayGet(values, i);
        if (!value || !xrtValueGetString(value, &items[i]) || !xl6TextSelect(&items[i], itemSize)) {
            xrtFree(items); return xl6TextFailure;
        }
    }
    size_t outputSize = 0;
    char* output = xrtStrJoinSized(separator, items, count, &outputSize);
    xrtFree(items);
    return xl6TextOwn(output, outputSize);
}
