#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include <xl_value_read.h>
#include <string.h>
extern void read_view_mutate(void);
static void foreignDrop(ptr raw, ptr context) { (void)raw; (void)context; }
int64 read_view_probe(xvalue* words, xvalue* options)
{
    XL_ValueReadV1 array = {0}, dict = {0};
    int64 result = 0;
    if (!xlValueReadBeginV1(xlValueBorrowV1(words), &array)) goto done;
    if (!xlValueReadBeginV1(xlValueBorrowV1(options), &dict)) goto done;
    if (array.kind != XVALUE_ARRAY || array.count != 2 || dict.kind != XVALUE_OBJECT) goto done;
    XL_ValueBorrowV1 first = xlValueReadAtV1(&array, 0, NULL);
    if (first.kind != XVALUE_STRING || first.ref || first.data.text.Size != 3 ||
        memcmp(first.data.text.Data, "a\0b", 3)) goto done;
    for (int i = 0; i < 2000; ++i) {
        XL_ValueBorrowV1 again = xlValueReadAtV1(&array, 0, NULL);
        if (again.data.text.Data != first.data.text.Data || again.data.text.Size != 3) goto done;
    }
    XL_ValueBorrowV1 name = xlValueReadFindV1(&dict, (xstrview){"name", 4});
    if (name.kind != XVALUE_STRING || name.ref || name.data.text.Size != 6 ||
        memcmp(name.data.text.Data, "native", 6)) goto done;
    XL_ValueBorrowV1 absent = xlValueReadFindV1(&dict, (xstrview){"absent", 6});
    if (absent.ref || absent.kind != XVALUE_NULL || xrtGetError()) goto done;
    /* Reentrant mutation failure is checked by a separate call below. */
    if (xrtGetError()) goto done;
    XL_ValueReadV1 copy = array;
    if (xlValueReadEndV1(&copy)) goto done;
    xrtClearError();
    (void)xlValueReadAtV1(&array, 2, NULL);
    if (!xrtGetError()) goto done;
    xrtClearError();
    static const xvaluehandleops foreign = {NULL, foreignDrop, NULL, NULL};
    ptr raw = (ptr)1;
    xvalue* unknown = xrtValueHandleTake(&raw, &foreign, NULL);
    if (!unknown) goto done;
    XL_ValueReadV1 rejected = {0};
    bool accepted = xlValueReadBeginV1(xlValueBorrowV1(unknown), &rejected);
    if (accepted) (void)xlValueReadEndV1(&rejected);
    bool diagnosed = xrtGetError() != NULL;
    xrtClearError(); xrtValueRelease(unknown);
    if (accepted || !diagnosed) goto done;
    result = 1;
done:
    if (!xlValueReadEndV1(&dict)) result = 0;
    if (!xlValueReadEndV1(&array)) result = 0;
    if (!xlValueReadEndV1(&array)) result = 0;
    return result;
}

int64 read_view_reenter(xvalue* words)
{
    XL_ValueReadV1 view = {0};
    if (!xlValueReadBeginV1(xlValueBorrowV1(words), &view)) return 0;
    read_view_mutate();
    /* Preserve the callback's error while releasing every native borrow.
     * The outer language catch observes the authoritative foreign-call failure. */
    xerror* failure = xrtTakeError();
    bool ended = xlValueReadEndV1(&view);
    if (failure) { xrtClearError(); xrtSetErrorTake(failure); }
    return ended ? 1 : 0;
}

/* COM's missing/IDispatch arguments can use array<point>, without boxing slots. */
static int addressToken;
ptr read_view_address(void) { return &addressToken; }
int64 read_view_pointer_probe(xvalue* values, ptr expected)
{
    XL_ValueReadV1 view = {0};
    int64 result = 0;
    if (!xlValueReadBeginV1(xlValueBorrowV1(values), &view)) return 0;
    XL_ValueBorrowV1 first = xlValueReadAtV1(&view, 0, NULL);
    XL_ValueBorrowV1 second = xlValueReadAtV1(&view, 1, NULL);
    if (view.count != 2 || first.kind != XVALUE_POINTER || first.ref ||
        (ptr)(uintptr_t)first.data.unsignedInteger != expected ||
        second.kind != XVALUE_POINTER || second.data.unsignedInteger != 0) goto done;
    xvalue* owned = xlValueBorrowCopyV1(first);
    ptr raw = NULL;
    if (owned && xrtValueGetPointer(owned, &raw) && raw == expected) result = 1;
    xrtValueRelease(owned);
done:
    if (!xlValueReadEndV1(&view)) result = 0;
    return result;
}
