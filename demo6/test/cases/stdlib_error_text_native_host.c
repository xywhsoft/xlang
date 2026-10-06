#include "src/runtime/xl6_value_format.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ERROR_ID UINT64_C(0xEC7E8C31BA4646DB)
static const char nulText[] = "a\0\xE4\xBD\xA0";
static void expectText(const xvalue* value, const char* expected, size_t size)
{
    xstrview text = xl6ValueTextV1(value);
    assert(text.Data && text.Size == size && !memcmp(text.Data, expected, size));
    xrtFree((void*)text.Data);
}
static xerror* makeError(void)
{ return xrtErrorCreate(XERR_VALUE, "native-test", 73, "invalid integer string"); }
static unsigned foreignDrops;
static void foreignDrop(void* payload, void* user)
{ (void)payload; (void)user; ++foreignDrops; }
static const xvaluehandleops foreignOps = {NULL, foreignDrop, NULL, NULL};
static void vectors(void)
{
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        xl6ProductMemoryBegin(SIZE_MAX);
        xerror* error = makeError(); assert(error);
        xvalue* boxed = xl6ErrorValueRefV1(error, ERROR_ID); assert(boxed);
        assert(xl6ErrorValueBorrowV1(boxed) == error);
        assert(xl6ErrorValueOwnershipAdapterV1(xrtValueOwnership(boxed)));
        expectText(boxed, "invalid integer string", 22);
        xstrbuf appended;
        xrtStrBufInit(&appended);
        assert(xl6ValueTextAppendV1(&appended, boxed));
        xstrview message = xrtStrBufView(&appended);
        assert(message.Size == 22 && !memcmp(message.Data, "invalid integer string", 22));
        xrtStrBufFree(&appended);
        xvalue* cloned = xrtValueClone(boxed); assert(cloned);
        assert(xl6ErrorValueBorrowV1(cloned) == error);
        assert(xl6ErrorValueOwnershipAdapterV1(xrtValueOwnership(cloned)));
        xerror* read = NULL;
        assert(xl6ErrorValueReadRefV1(cloned, ERROR_ID, &read) && read == error);
        xrtErrorFree(read);
        read = error;
        assert(!xl6ErrorValueReadRefV1(cloned, ERROR_ID + 1, &read) && read == error);
        assert(xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
        assert(xl6ErrorValueReadRefV1(xrtValueNull(), ERROR_ID, &read) && !read);
        xvalue* absent = xl6ErrorValueRefV1(NULL, ERROR_ID); assert(absent);
        expectText(absent, "null", 4); xrtValueRelease(absent);
        /* Same nominal identity but unrelated payload/operations must never
         * be interpreted as an Error or accepted for graph retirement. */
        void* raw = (void*)(uintptr_t)1;
        xvalue* fake = xrtValueHandleTake(&raw, &foreignOps, NULL); assert(fake && !raw);
        assert(xrtValueTypeIdBind(fake, ERROR_ID));
        xrtSetErrorKind(XERR_IO);
        assert(!xl6ErrorValueBorrowV1(fake));
        assert(xrtErrorKind(xrtGetError()) == XERR_IO); xrtClearError();
        assert(!xl6ErrorValueOwnershipAdapterV1(xrtValueOwnership(fake)));
        read = error;
        assert(!xl6ErrorValueReadRefV1(fake, ERROR_ID, &read) && read == error);
        assert(xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
        assert(!xl6ValueTextV1(fake).Data);
        assert(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED); xrtClearError();
        xrtValueRelease(fake);
        const xvaluehandleops* trustedOps = NULL;
        void* originalPayload = NULL;
        assert(xrtValueGetHandle(boxed, &originalPayload, &trustedOps, NULL));
        raw = xrtErrorRef(error);
        fake = xrtValueHandleTake(&raw, trustedOps, NULL); assert(fake && !raw);
        assert(xrtValueTypeIdBind(fake, ERROR_ID));
        assert(!xl6ErrorValueBorrowV1(fake));
        assert(!xl6ErrorValueOwnershipAdapterV1(xrtValueOwnership(fake)));
        read = error;
        assert(!xl6ErrorValueReadRefV1(fake, ERROR_ID, &read) && read == error);
        assert(xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
        xrtValueRelease(fake);
        xvalue* text = xrtValueString((xstrview){nulText, sizeof(nulText)-1}); assert(text);
        expectText(text, nulText, sizeof(nulText)-1); xrtValueRelease(text);
        xvalue* scalar = xrtValueChar(0); assert(scalar);
        expectText(scalar, "\0", 1); xrtValueRelease(scalar);
        scalar = xrtValueChar(0x4F60); assert(scalar);
        expectText(scalar, "\xE4\xBD\xA0", 3); xrtValueRelease(scalar);
        scalar = xrtValueUInt(UINT64_MAX); assert(scalar);
        expectText(scalar, "18446744073709551615", 20); xrtValueRelease(scalar);
        scalar = xrtValueInt(INT64_MIN); assert(scalar);
        expectText(scalar, "-9223372036854775808", 20); xrtValueRelease(scalar);
        scalar = xrtValueBool(true); assert(scalar);
        expectText(scalar, "true", 4); xrtValueRelease(scalar);
        xrtValueRelease(boxed); xrtValueRelease(cloned); xrtErrorFree(error);
        assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    }
    assert(foreignDrops == 100);
    puts("error text: 100 exact identity/clone/graph/NUL/char/scalar cycles balanced");
}
static void fault(const char* action, size_t point)
{
    xerror* error = makeError(); assert(error);
    xvalue* boxed = xl6ErrorValueRefV1(error, ERROR_ID); assert(boxed);
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point));
    xvalue* result = NULL;
    xstrview text = {0};
    xstrbuf appended;
    xrtStrBufInit(&appended);
    bool appendedOk = false;
    if (!strcmp(action, "box")) result = xl6ErrorValueRefV1(error, ERROR_ID);
    else if (!strcmp(action, "clone")) result = xrtValueClone(boxed);
    else if (!strcmp(action, "text")) text = xl6ValueTextV1(boxed);
    else { assert(!strcmp(action, "append")); appendedOk = xl6ValueTextAppendV1(&appended, boxed); }
    bool hit = xl6ProductMemoryEnd();
    xerror* failure = xrtTakeError();
    assert(hit ? !result && !text.Data && !appendedOk && xrtErrorKind(failure) == XERR_MEMORY :
        !failure && (result || text.Data || appendedOk));
    if (!strcmp(action, "append")) assert(xrtStrBufView(&appended).Size == (hit ? 0u : 22u));
    assert(xl6ErrorValueBorrowV1(boxed) == error);
    assert(!strcmp(xrtErrorMessage(error), "invalid integer string"));
    xrtValueRelease(result); xrtFree((void*)text.Data); xrtErrorFree(failure);
    xrtStrBufFree(&appended);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    xrtValueRelease(boxed); xrtErrorFree(error);
    printf("error text %s point=%zu hit=%d balanced\n", action, point, hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc == 1) vectors();
    else { assert(argc == 3); fault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); }
    return 0;
}
