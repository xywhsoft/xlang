#include "src/runtime/xl6_text_internal.h"
#include "src/runtime/xl6_array_input.h"

xstrview xl6TextJoinV1(const xvalue* values, xstrview separator, int64_t separatorSize, int64_t itemSize)
{
    if (!xl6TextSelect(&separator, separatorSize)) return xl6TextFailure;
    if (itemSize < -1) { xrtSetErrorKind(XERR_RANGE); return xl6TextFailure; }
    XL6_ArrayInputV1 input={0};
    if (!xl6ArrayInputAcquireV1(values,&input)) return xl6TextFailure;
    size_t count = input.count;
    if (count > SIZE_MAX / sizeof(xstrview)) { xrtSetErrorKind(XERR_RANGE); goto fail; }
    xstrview* items = count ? xrtMalloc(count * sizeof(*items)) : NULL;
    if (count && !items) { xrtSetErrorKind(XERR_MEMORY); goto fail; }
    for (size_t i = 0; i < count; ++i) {
        const xvalue* value = input.items[i];
        if (!value || !xrtValueGetString(value, &items[i]) || !xl6TextSelect(&items[i], itemSize)) {
            xrtFree(items); goto fail;
        }
    }
    size_t outputSize = 0;
    char* output = xrtStrJoinSized(separator, items, count, &outputSize);
    xrtFree(items);
    xl6ArrayInputReleaseV1(&input);
    return xl6TextOwn(output, outputSize);
fail:
    xl6ArrayInputReleaseV1(&input); return xl6TextFailure;
}
