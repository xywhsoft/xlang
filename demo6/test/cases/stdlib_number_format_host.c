#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct Fault {size_t calls, fail; bool hit;} Fault;
static void* allocate(void* context, size_t size)
{
    Fault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void release(void* context, void* pointer) {(void)context; free(pointer);}
static void check(xstrview result, const char* expected, size_t size)
{
    assert(result.Data && result.Size == size && !memcmp(result.Data, expected, size));
    assert(result.Data[size] == 0); xrtFree((void*)result.Data);
}
int main(int argc, char** argv)
{
    static Fault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    if (argc == 3) {
        size_t mode = (size_t)strtoull(argv[1], NULL, 10);
        size_t point = (size_t)strtoull(argv[2], NULL, 10);
        xbuffer* buffer = NULL;
        if (mode >= 3) {
            buffer = xrtBufferCreate(); assert(buffer);
            assert(xrtBufferAppend(buffer, (xbytesview){(const uint8*)"prefix", 6}));
        }
        fault.calls = 0; fault.fail = point;
        xstrview format = XRT_STR_LITERAL("8192");
        xstrview text = {NULL, 0}; bool ok;
        if (mode == 0) text = xl6NumberIntFormatV1(INT64_MIN, format, -1);
        else if (mode == 1) text = xl6NumberUIntFormatV1(UINT64_MAX, format, -1);
        else if (mode == 2) text = xl6NumberFloatFormatV1(DBL_MAX, format, -1);
        else if (mode == 3) ok = xl6NumberBufferIntFormatV1(buffer, INT64_MIN, format, -1);
        else if (mode == 4) ok = xl6NumberBufferUIntFormatV1(buffer, UINT64_MAX, format, -1);
        else {assert(mode == 5); ok = xl6NumberBufferFloatFormatV1(buffer, DBL_MAX, format, -1);}
        if (mode < 3) ok = text.Data != NULL;
        if (fault.hit) {
            assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            assert(text.Data == NULL && text.Size == 0);
            if (buffer) assert(buffer->Size == 6 && !memcmp(buffer->Data, "prefix", 6));
        } else {
            assert(ok);
            if (buffer) assert(buffer->Size == 8198);
            else assert(text.Size == 8192);
        }
        fault.fail = SIZE_MAX;
        xrtFree((void*)text.Data); xrtBufferDestroy(buffer); xrtClearError();
        assert(xrtMemDebugReset());
        printf("numeric allocation mode=%zu point=%zu calls=%zu hit=%d balanced\n",mode,point,fault.calls,fault.hit);
        return 0;
    }
    assert(argc == 1);
    check(xl6NumberIntFormatV1(0, XRT_STR_LITERAL("c"), -1), "\0", 1);
    check(xl6NumberIntFormatV1(0, XRT_STR_LITERAL("4c"), -1), "   \0", 4);
    check(xl6NumberIntFormatV1(INT64_MIN, XRT_STR_LITERAL(""), -1), "-9223372036854775808", 20);
    check(xl6NumberUIntFormatV1(UINT64_MAX, XRT_STR_LITERAL("X"), -1), "FFFFFFFFFFFFFFFF", 16);
    check(xl6NumberFloatFormatV1(-0.0, XRT_STR_LITERAL(".2f"), -1), "-0.00", 5);
    check(xl6NumberIntFormatV1(65, XRT_STR_LITERAL("c\0tail"), 1), "A", 1);
    check(xl6NumberIntFormatV1(65, XRT_STR_LITERAL("?"), 0), "65", 2);
    check(xl6NumberIntFormatV1(65, XRT_STR_LITERAL("c"), INT64_MAX), "A", 1);
    xstrview text = xl6NumberFloatFormatV1(DBL_MAX, XRT_STR_LITERAL(".1000f"), -1);
    assert(text.Data && text.Size == 1310); xrtFree((void*)text.Data);
    text = xl6NumberIntFormatV1(1, XRT_STR_LITERAL("d"), -2);
    assert(!text.Data && !text.Size && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    text = xl6NumberIntFormatV1(1, (xstrview){NULL, 1}, 0);
    assert(!text.Data && !text.Size && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    text = xl6NumberIntFormatV1(1, XRT_STR_LITERAL("d\0"), -1);
    assert(!text.Data && !text.Size && xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
    xbuffer* buffer = xrtBufferCreate(); assert(buffer);
    assert(xrtBufferAppend(buffer, (xbytesview){(const uint8*)"c", 1}));
    xstrview alias = {(const char*)buffer->Data, 1};
    assert(xl6NumberBufferIntFormatV1(buffer, 0, alias, -1));
    assert(buffer->Size == 2 && buffer->Data[1] == 0);
    assert(!xl6NumberBufferFloatFormatV1(buffer, 1, XRT_STR_LITERAL("?"), -1));
    assert(buffer->Size == 2 && xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
    assert(!xl6NumberBufferIntFormatV1(NULL, 1, XRT_STR_LITERAL("d"), -1));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xrtBufferDestroy(buffer); assert(xrtMemDebugReset());
    puts("number format exact lengths, aliasing, boundaries passed");
    return 0;
}
