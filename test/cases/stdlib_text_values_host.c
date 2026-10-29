#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void equal(xstrview result, xstrview expected)
{
    assert(result.Data && result.Size == expected.Size);
    assert(!expected.Size || !memcmp(result.Data, expected.Data, expected.Size));
    assert(result.Data[result.Size] == 0);
    xrtFree((void*)result.Data);
}
typedef struct Fault { size_t calls, fail, live; bool hit; } Fault;
static void* allocate(void* context, size_t size)
{
    Fault* f = context;
    if (++f->calls == f->fail) { f->hit = true; return NULL; }
    void* p = malloc(size); if (p) ++f->live; return p;
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* f = context;
    if (++f->calls == f->fail) { f->hit = true; return NULL; }
    if (!size) { if (pointer) --f->live; free(pointer); return NULL; }
    bool fresh = !pointer; void* p = realloc(pointer, size);
    if (p && fresh) ++f->live;
    return p;
}
static void deallocate(void* context, void* pointer)
{
    if (pointer) { Fault* f = context; assert(f->live); --f->live; free(pointer); }
}
static xstrview operation(unsigned id)
{
    /* Force backing allocations rather than the <=1024-byte heap cache. */
    static char large[4096];
    memset(large, 'x', sizeof(large));
    memcpy(large, " A\0你B ", 8);
    xstrview text = {large, sizeof(large)};
    xstrview part = XRT_STR_LITERAL("A");
    switch (id) {
    case 0: return xl6TextLowerV1(text, -1);
    case 1: return xl6TextUpperV1(text, -1);
    case 2: return xl6TextTrimV1(text, XRT_STR_LITERAL(""), -1, -1);
    case 3: return xl6TextLTrimV1(text, part, -1, -1);
    case 4: return xl6TextRTrimV1(text, part, -1, -1);
    case 5: return xl6TextFilterV1(text, part, -1, -1);
    case 6: return xl6TextReplaceV1(text, part, XRT_STR_LITERAL("你\0"), -1, -1, -1);
    case 7: return xl6TextSliceV1(text, 1, 3000, -1);
    case 8: return xl6TextLeftV1(text, 3000, -1);
    case 9: return xl6TextRightV1(text, 3000, -1);
    case 10: return xl6TextRepeatV1(text, 3, -1);
    case 11: return xl6TextReverseV1(text, -1);
    case 12: return xl6TextRemoveAtV1(text, 1, 2, -1);
    case 13: return xl6TextInsertV1(text, 1, part, -1, -1);
    case 14: return xl6TextRemoveV1(text, part, false, -1, -1);
    case 15: return xl6TextRemoveV1(text, part, true, -1, -1);
    case 16: return xl6TextPadLeftV1(text, 6000, XRT_STR_LITERAL("你\0"), -1, -1);
    case 17: return xl6TextPadRightV1(text, 6000, XRT_STR_LITERAL("你\0"), -1, -1);
    default: return xl6TextPadCenterV1(text, 6000, XRT_STR_LITERAL("你\0"), -1, -1);
    }
}
int main(void)
{
    static Fault fault = {0, SIZE_MAX, 0, false};
    xallocator allocator = {&fault, allocate, resize, deallocate};
    /* XRT freezes allocator selection on first allocation, even if freed. */
    assert(xrtSetAllocator(&allocator));
    equal(xl6TextLowerV1(XRT_STR_LITERAL("A\0你B"), -1), XRT_STR_LITERAL("a\0你b"));
    equal(xl6TextUpperV1(XRT_STR_LITERAL("a\0你b"), -1), XRT_STR_LITERAL("A\0你B"));
    equal(xl6TextTrimV1(XRT_STR_LITERAL("  A\0B \t"), XRT_STR_LITERAL(""), -1, -1), XRT_STR_LITERAL("A\0B"));
    equal(xl6TextFilterV1(XRT_STR_LITERAL("你A\0你B"), XRT_STR_LITERAL("你"), -1, -1), XRT_STR_LITERAL("A\0B"));
    equal(xl6TextReplaceV1(XRT_STR_LITERAL("a\0b\0a"), XRT_STR_LITERAL("a"), XRT_STR_LITERAL("你\0"), -1, -1, -1), XRT_STR_LITERAL("你\0\0b\0你\0"));
    equal(xl6TextSliceV1(XRT_STR_LITERAL("a\0你b"), 1, 2, -1), XRT_STR_LITERAL("\0你"));
    equal(xl6TextReverseV1(XRT_STR_LITERAL("a\0你b"), -1), XRT_STR_LITERAL("b你\0a"));
    equal(xl6TextRemoveAtV1(XRT_STR_LITERAL("a\0你b"), 1, 1, -1), XRT_STR_LITERAL("a你b"));
    equal(xl6TextInsertV1(XRT_STR_LITERAL("a\0b"), 1, XRT_STR_LITERAL("你\0"), -1, -1), XRT_STR_LITERAL("a你\0\0b"));
    equal(xl6TextRepeatV1(XRT_STR_LITERAL("a\0b"), 0, -1), XRT_STR_LITERAL(""));
    equal(xl6TextRepeatV1(XRT_STR_LITERAL("a\0b"), 2, -1), XRT_STR_LITERAL("a\0ba\0b"));
    equal(xl6TextRightV1(XRT_STR_LITERAL("a\0b"), 0, -1), XRT_STR_LITERAL(""));
    equal(xl6TextRightV1(XRT_STR_LITERAL("a\0b"), INT64_MAX, -1), XRT_STR_LITERAL("a\0b"));
    equal(xl6TextLowerV1(XRT_STR_LITERAL("A\0B"), 20), XRT_STR_LITERAL("a\0b"));
    equal(xl6TextLowerV1(XRT_STR_LITERAL("你A"), 0), XRT_STR_LITERAL(""));
    equal(xl6TextRemoveV1(XRT_STR_LITERAL("A\0bA\0"), XRT_STR_LITERAL("a"), true, -1, -1), XRT_STR_LITERAL("\0b\0"));
    equal(xl6TextPadLeftV1(XRT_STR_LITERAL("x\0"), 4, XRT_STR_LITERAL("你\0"), -1, -1), XRT_STR_LITERAL("你\0x\0"));
    equal(xl6TextPadRightV1(XRT_STR_LITERAL("x\0"), 4, XRT_STR_LITERAL("你\0"), -1, -1), XRT_STR_LITERAL("x\0你\0"));
    equal(xl6TextPadCenterV1(XRT_STR_LITERAL("x\0"), 4, XRT_STR_LITERAL("你\0"), -1, -1), XRT_STR_LITERAL("你x\0你"));
    xrtClearError(); assert(!xl6TextLowerV1(XRT_STR_LITERAL("你A"), 1).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError(); assert(!xl6TextLowerV1((xstrview){NULL,1}, -1).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError(); assert(!xl6TextLowerV1(XRT_STR_LITERAL("A"), -2).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError(); assert(!xl6TextRepeatV1(XRT_STR_LITERAL("A"), -1, -1).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError(); assert(!xl6TextRepeatV1(XRT_STR_LITERAL("AB"), INT64_MAX, -1).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError(); assert(!xl6TextSliceV1(XRT_STR_LITERAL("AB"), 0, -2, -1).Data && xrtErrorKind(xrtGetError()) != XERR_NONE);
    xrtClearError();
    assert(xrtMemDebugReset());
    unsigned hits = 0;
    size_t retained = fault.live; /* Physical cached spans, no logical values. */
    for (unsigned id = 0; id < 19; ++id) {
        fault.fail = SIZE_MAX;
        xstrview success = operation(id);
        assert(success.Data); xrtFree((void*)success.Data);
        assert(xrtMemDebugReset());
        size_t baseline = fault.live;
        fault.fail = fault.calls + 1; fault.hit = false;
        xstrview failure = operation(id);
        if (!(fault.hit && !failure.Data && !failure.Size && xrtErrorKind(xrtGetError()) != XERR_NONE))
            fprintf(stderr, "OOM probe id=%u hit=%d result=%p size=%zu kind=%d calls=%zu fail=%zu\n",
                id, fault.hit, (const void*)failure.Data, failure.Size, xrtErrorKind(xrtGetError()), fault.calls, fault.fail);
        assert(fault.hit && !failure.Data && !failure.Size && xrtErrorKind(xrtGetError()) != XERR_NONE);
        ++hits; xrtClearError(); assert(xrtMemDebugReset());
        assert(fault.live == baseline);
    }
    assert(hits == 19 && fault.live == retained);
    printf("text values: exact bytes, UTF-8, boundaries; %u actual OOM paths balanced\n", hits);
    return 0;
}
