#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <xl_value_read.h>
#include <string.h>
extern void* xv_build(xvalue* card);

/* Isolate input reads from the output buffer's necessary allocations. */
int64 vcard_read_noalloc(xvalue* card)
{
    XL_ValueReadV1 object = {0}, array = {0};
    int64 ok = 0;
    if (!xrtMemDebugFailAfter(0)) return 0;
    if (!xlValueReadBeginV1(xlValueBorrowV1(card), &object) || object.kind != XVALUE_OBJECT) goto done;
    XL_ValueBorrowV1 fn = xlValueReadFindV1(&object, (xstrview){"fn",2});
    if (fn.kind != XVALUE_STRING || fn.data.text.Size != 1 || fn.data.text.Data[0] != 'A') goto done;
    XL_ValueBorrowV1 emails = xlValueReadFindV1(&object, (xstrview){"email",5});
    if (!xlValueReadBeginV1(emails, &array) || array.kind != XVALUE_ARRAY || array.count != 2) goto done;
    XL_ValueBorrowV1 first = xlValueReadAtV1(&array,0,NULL);
    if (first.kind != XVALUE_STRING || first.ref || first.data.text.Size != 1 || first.data.text.Data[0] != 'a') goto done;
    for (size_t i=0; i<2000; ++i) {
        XL_ValueBorrowV1 again = xlValueReadAtV1(&array,0,NULL);
        if (again.ref || again.data.text.Data != first.data.text.Data || again.data.text.Size != first.data.text.Size) goto done;
    }
    ok = !xrtMemDebugFailTriggered() && !xrtGetError();
done:
    xrtMemDebugFailClear();
    if (!xlValueReadEndV1(&array)) ok = 0;
    if (!xlValueReadEndV1(&object)) ok = 0;
    return ok;
}

int64 vcard_build_allocations(xvalue* card)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    void* text = xv_build(card);
    xrtMemDebugSnapshot(&after);
    if (!text || !before.Enabled || !after.Enabled) { xrtFree(text); return -1; }
    xrtFree(text);
    return (int64)((after.AllocCount-before.AllocCount)*10000 + after.ReallocCount-before.ReallocCount);
}

static void drop_unknown(ptr raw, ptr user) { (void)raw; (void)user; }
int64 vcard_unknown_rejected(void)
{
    static const xvaluehandleops ops = {NULL, drop_unknown, NULL, NULL};
    ptr raw = (ptr)1;
    xvalue* unknown = xrtValueHandleTake(&raw, &ops, NULL);
    if (!unknown) return 0;
    void* result = xv_build(unknown);
    int64 rejected = !result && xrtGetError() != NULL;
    xrtClearError(); xrtFree(result); xrtValueRelease(unknown);
    return rejected;
}
