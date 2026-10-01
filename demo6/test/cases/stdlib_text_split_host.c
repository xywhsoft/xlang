#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
static void itemEquals(const xvalue* array, size_t index, xstrview expected)
{
    const xvalue* item = xrtValueArrayGet(array, index);
    xstrview actual;
    assert(item && xrtValueType(item) == XVALUE_STRING && xrtValueGetString(item, &actual));
    assert(actual.Size == expected.Size && (!actual.Size || !memcmp(actual.Data, expected.Data, actual.Size)));
}
static void memoryFault(const char* mode, size_t point)
{
    static Fault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    bool large = strstr(mode, "-large") != NULL;
    bool lines = !strncmp(mode, "lines", 5), fields = !strncmp(mode, "fields", 6), join = !strncmp(mode, "join", 4);
    size_t width = large ? 4096 : 4, size = 64 * width;
    char data[64 * 4096];
    memset(data, 'a', size);
    for (size_t i = 0; i < size; i += width) {
        data[i] = 'a'; data[i+1] = 0; data[i+2] = 'b';
        data[i+width-1] = lines ? '\n' : fields ? ' ' : '|';
    }
    xvalue* input = NULL;
    if (join) {
        input = xrtValueArray(); assert(input);
        for (size_t i = 0; i < 64; ++i) assert(xrtValueArrayAppendNew(input, xrtValueString((xstrview){data, width-1})));
    }
    fault.calls = 0; fault.fail = point;
    xstrview text = {data, size}, joined = {NULL, 0};
    xvalue* result = NULL;
    if (!strncmp(mode, "split", 5)) result = xl6TextSplitV1(text, XRT_STR_LITERAL("|"), -1, -1);
    else if (lines) result = xl6TextLinesV1(text, -1);
    else if (fields) result = xl6TextFieldsV1(text, -1);
    else { assert(join); joined = xl6TextJoinV1(input, XRT_STR_LITERAL("|\0"), -1, -1); }
    if (fault.hit) {
        assert(!result && !joined.Data && joined.Size == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    } else if (input) {
        assert(joined.Data && joined.Size == 64 * (width-1) + 126);
    } else assert(result && xrtValueCount(result) == (!strncmp(mode, "split", 5) ? 65 : 64));
    xrtFree((void*)joined.Data); xrtValueRelease(result); xrtValueRelease(input); xrtClearError();
    assert(xrtMemDebugReset());
    printf("%s allocation point=%zu calls=%zu hit=%d balanced\n", mode, point, fault.calls, fault.hit);
}
int main(int argc, char** argv)
{
    if (argc == 3) {memoryFault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); return 0;}
    assert(argc == 1);
    char data[] = "你\0|😀||a\0|";
    xvalue* result = xl6TextSplitV1((xstrview){data, sizeof(data)-1}, XRT_STR_LITERAL("|"), -1, -1);
    assert(result && xrtValueCount(result) == 5);
    memset(data, 'x', sizeof(data)-1);
    itemEquals(result, 0, XRT_STR_LITERAL("你\0"));
    itemEquals(result, 1, XRT_STR_LITERAL("😀")); itemEquals(result, 2, XRT_STR_LITERAL(""));
    itemEquals(result, 3, XRT_STR_LITERAL("a\0")); itemEquals(result, 4, XRT_STR_LITERAL(""));
    xstrview joined = xl6TextJoinV1(result, XRT_STR_LITERAL("|"), -1, -1);
    assert(joined.Data && joined.Size == 14 && !memcmp(joined.Data, "你\0|😀||a\0|", 14));
    xrtValueRelease(result); assert(joined.Data[0] == (char)0xe4); xrtFree((void*)joined.Data);
    result = xl6TextLinesV1(XRT_STR_LITERAL("a\0\r\n\r你\n"), -1);
    assert(result && xrtValueCount(result) == 3);
    itemEquals(result, 0, XRT_STR_LITERAL("a\0")); itemEquals(result, 1, XRT_STR_LITERAL(""));
    itemEquals(result, 2, XRT_STR_LITERAL("你")); xrtValueRelease(result);
    result = xl6TextFieldsV1(XRT_STR_LITERAL(" \t你\0😀\v\fx "), -1);
    assert(result && xrtValueCount(result) == 2); itemEquals(result, 0, XRT_STR_LITERAL("你\0😀"));
    itemEquals(result, 1, XRT_STR_LITERAL("x")); xrtValueRelease(result);
    result = xl6TextSplitV1(XRT_STR_LITERAL(""), XRT_STR_LITERAL(""), -1, -1);
    assert(result && xrtValueCount(result) == 1); itemEquals(result, 0, XRT_STR_LITERAL("")); xrtValueRelease(result);
    result = xl6TextFieldsV1(XRT_STR_LITERAL(" \t"), -1);
    joined = xl6TextJoinV1(result, XRT_STR_LITERAL("|"), -1, -1);
    assert(joined.Data && !joined.Size && !joined.Data[0]); xrtFree((void*)joined.Data);
    assert(!xl6TextJoinV1(result, XRT_STR_LITERAL("|"), -1, -2).Data && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError(); xrtValueRelease(result);
    assert(!xl6TextSplitV1(XRT_STR_LITERAL("你"), XRT_STR_LITERAL("|"), 1, -1) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    assert(!xl6TextLinesV1((xstrview){NULL, 5}, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    result = xrtValueArray(); assert(result && xrtValueArrayAppendNew(result, xrtValueInt(65)));
    assert(!xl6TextJoinV1(result, XRT_STR_LITERAL("|"), -1, -1).Data && xrtErrorKind(xrtGetError()) == XERR_TYPE);
    xrtClearError(); xrtValueRelease(result);
    assert(xrtMemDebugReset());
    puts("text split: owned NUL/UTF8/arrays/lines/fields/join/errors balanced");
    return 0;
}
