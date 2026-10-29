#include "src/runtime/xl6_text_value.h"
#include "test/support/array_producer_test.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Fault {size_t calls, fail; bool hit;} Fault;
static void* allocate(void* context, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void release(void* context, void* pointer) {(void)context; free(pointer);}
static void memoryFault(const char* name, size_t point)
{
    static Fault fault;
    fault.fail = point;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    char data[4096]; memset(data, 'a', sizeof(data));
    xstrview text = {data, sizeof(data)};
    xvalue* result = NULL;
    if (!strcmp(name, "bytes")) result = xl6TextBytesV1(text, -1);
    else if (!strcmp(name, "chars")) result = xl6TextCharsV2(XL6_TEST_CHAR_ARRAY_IDS,text, -1);
    else {
        assert(!strcmp(name, "distance"));
        char other[4096]; memset(other, 'b', sizeof(other));
        int64_t distance = xl6TextDistanceV1(text, (xstrview){other, sizeof(other)}, -1, -1, -1);
        assert(distance == (fault.hit ? -1 : (int64_t)sizeof(data)));
    }
    if (fault.hit) {
        if (result || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
            fprintf(stderr, "%s point=%zu result=%p kind=%d calls=%zu\n", name, point,
                (void*)result, xrtErrorKind(xrtGetError()), fault.calls);
        assert(result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    } else if (strcmp(name, "distance")) assert(result);
    xrtValueRelease(result);
    xrtClearError();
    assert(xrtMemDebugReset()); /* No partially created values/backing survived. */
    printf("%s allocation point=%zu calls=%zu hit=%d balanced\n", name, point, fault.calls, fault.hit);
}
int main(int argc, char** argv)
{
    if (argc == 3) {memoryFault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); return 0;}
    assert(argc == 1);
    xstrview text = XRT_STR_LITERAL("A\0你😀bA\0");
    assert(xl6TextLengthV1(text, -1) == 7 && xl6TextByteLengthV1(text, -1) == 12);
    assert(xl6TextLengthV1(text, 5) == 3 && xl6TextByteLengthV1(text, 3) == 3);
    assert(xl6TextCharAtV1(text, 2, -1) == 0x4F60 && xl6TextCharAtV1(text, -3, -1) == 'b');
    assert(xl6TextCharAtV1(text, 1, -1) == 0 && xl6TextByteAtV1(text, -1, -1) == 0);
    assert(xl6TextByteAtV1(XRT_STR_LITERAL("你"), 0, 1) == 228);
    assert(xl6TextCompareV1(XRT_STR_LITERAL("a\0b"), XRT_STR_LITERAL("a\0c"), false, -1, -1) < 0);
    assert(xl6TextEqualsV1(text, XRT_STR_LITERAL("a\0你😀Ba\0"), true, -1, -1));
    assert(xl6TextContainsV1(text, XRT_STR_LITERAL("\0你"), false, -1, -1));
    assert(xl6TextIndexOfV1(text, XRT_STR_LITERAL("你"), false, -1, -1) == 2);
    assert(xl6TextLastIndexOfV1(text, XRT_STR_LITERAL("a\0"), true, -1, -1) == 5);
    assert(xl6TextCountV1(text, XRT_STR_LITERAL("a\0"), true, -1, -1) == 2);
    assert(xl6TextStartsWithV1(text, XRT_STR_LITERAL("a\0"), true, -1, -1));
    assert(xl6TextEndsWithV1(text, XRT_STR_LITERAL("a\0"), true, -1, -1));
    assert(xl6TextHasAnyV1(text, XRT_STR_LITERAL("\0"), -1, -1));
    assert(xl6TextIsEmptyV1(text, 0) && xl6TextIsBlankV1(XRT_STR_LITERAL(" \t\r\n\v\f"), -1));
    assert(!xl6TextIsBlankV1(XRT_STR_LITERAL("\0"), -1));
    assert(xl6TextIsUtf8V1(text, -1) && !xl6TextIsUtf8V1(XRT_STR_LITERAL("你"), 1));
    assert(xrtErrorKind(xrtGetError()) == XERR_NONE);
    xvalue* binary = xl6TextBytesV1(text, -1);
    assert(binary && xrtValueType(binary) == XVALUE_BYTES);
    xbytesview bytes;
    assert(xrtValueGetBytes(binary, &bytes));
    assert(bytes.Size == text.Size && !memcmp(bytes.Data, text.Data, text.Size));
    xrtValueRelease(binary);
    binary = xl6TextBytesV1(XRT_STR_LITERAL("你"), 1);
    assert(xrtValueGetBytes(binary, &bytes));
    assert(bytes.Size == 1 && bytes.Data[0] == 228);
    xrtValueRelease(binary);
    xvalue* chars = xl6TextCharsV2(XL6_TEST_CHAR_ARRAY_IDS,text, -1);
    assert(chars && xl6TestArrayCount(chars) == 7);
    const uint32_t expected[] = {'A', 0, 0x4F60, 0x1F600, 'b', 'A', 0};
    for (size_t i = 0; i < 7; ++i) {
        assert(xl6TestArrayCharAt(chars,i)==expected[i]);
    }
    xrtValueRelease(chars);
    assert(xl6TextGlobV1(XRT_STR_LITERAL("a\0b"), XRT_STR_LITERAL("a?b"), 0, -1, -1));
    assert(xl6TextLikeV1(XRT_STR_LITERAL("README.MD"), XRT_STR_LITERAL("*.md"), true, -1, -1));
    assert(xl6TextDistanceV1(XRT_STR_LITERAL("你😀"), XRT_STR_LITERAL("你x"), -1, -1, -1) == 1);
    assert(xl6TextDistanceV1(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("xyz"), 2, -1, -1) == -1);
    assert(xrtErrorKind(xrtGetError()) == XERR_NONE);
    assert(xl6TextSimilarityV1(text, text, -1, -1) == 1);
    assert(xl6TextApproxV1(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("axc"), .5, -1, -1));
    xrtClearError();
    assert(xl6TextCharAtV1(text, INT64_MIN, -1) == 0 && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(xl6TextByteAtV1(text, INT64_MIN, -1) == 0 && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(!xl6TextCharsV2(XL6_TEST_CHAR_ARRAY_IDS,XRT_STR_LITERAL("你"), 1) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    assert(!xl6TextBytesV1((xstrview){NULL, 10}, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    assert(!xl6TextApproxV1(text, text, -1, -1, -1) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(xrtMemDebugReset());
    puts("text query: scalar/byte/tag/NUL/caps/UTF/errors/lifetimes balanced");
    return 0;
}
