#include "foreign_xge_utf8_read.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureUtf8NativeCases(void)
{
    static const char valid[] = "A\xc2\xa2\xe6\x96\x87\xf0\x9f\x99\x82";
    static const size_t offsets[] = { 0, 1, 3, 6, 10 };
    static const uint32_t points[] = { 'A', 0xa2, 0x6587, 0x1f642 };
    const char* cursor = valid;
    uint32_t codepoint = 0;
    size_t next = 0;
    for (size_t i = 0; i < 4; ++i) {
        assert(xgeTextUTF8Read(valid, sizeof(valid) - 1, offsets[i], &codepoint, &next) == XGE_OK);
        assert(codepoint == points[i] && next == offsets[i + 1]);
        assert(xgeTextUTF8Next(&cursor, &codepoint) == XGE_OK);
        assert(codepoint == points[i] && cursor == valid + next);
    }
    assert(xgeTextUTF8Next(&cursor, &codepoint) == XGE_ERROR && cursor == valid + 10);
    assert(xgeTextUTF8Read(valid, sizeof(valid), 10, &codepoint, &next) == XGE_OK);
    assert(codepoint == 0 && next == 11);
    assert(xgeTextUTF8Read(NULL, 0, 0, &codepoint, &next) == XGE_OK);
    assert(codepoint == 0 && next == 0);

    static const char* const bad[] = { "\x80", "\xc0\xaf", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80" };
    static const size_t sizes[] = { 1, 2, 2, 3, 4 };
    codepoint = 23;
    next = 72;
    for (size_t i = 0; i < 5; ++i) {
        cursor = bad[i];
        assert(xgeTextUTF8Read(bad[i], sizes[i], 0, &codepoint, &next) == XGE_ERROR_UNSUPPORTED);
        assert(xgeTextUTF8Next(&cursor, &codepoint) == XGE_ERROR_UNSUPPORTED);
        assert(codepoint == 23 && next == 72 && cursor == bad[i]);
    }
    assert(xgeTextUTF8Read(valid, 1, 2, &codepoint, &next) == XGE_ERROR_INVALID_ARGUMENT);
    assert(xgeTextUTF8Read(valid, 1, 0, NULL, &next) == XGE_ERROR_INVALID_ARGUMENT);
    assert(xgeTextUTF8Read(NULL, 1, 0, &codepoint, &next) == XGE_ERROR_INVALID_ARGUMENT);
    assert(codepoint == 23 && next == 72);
}

void fixtureUtf8BeginSample(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureUtf8EndSample(int language)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    printf("UTF-8 heap %s: alloc=%llu free=%llu realloc=%llu\n",
        language ? "language" : "native",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void fixtureUtf8NativeSample(void)
{
    static const char text[] = "A\xc2\xa2\xe6\x96\x87\xf0\x9f\x99\x82";
    fixtureUtf8BeginSample();
    for (int i = 0; i < 256; ++i) {
        uint32_t codepoint;
        size_t next;
        assert(xgeTextUTF8Read(text, sizeof(text) - 1, 3, &codepoint, &next) == XGE_OK);
        assert(codepoint == 0x6587 && next == 6);
    }
    fixtureUtf8EndSample(0);
}
