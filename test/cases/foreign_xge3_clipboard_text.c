#include "xge3_runtime.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xmemdebugsnapshot before;

int clipboardFixtureRealDisplay(void)
{
#if defined(__linux__)
    const char* isolated = getenv("XGE_CLIPBOARD_PRIVATE_DISPLAY");
    return isolated && !strcmp(isolated,"1");
#else
    return 0;
#endif
}

void clipboardFixtureBudget(int count)
{
    if (count < 0) {
        xrtMemDebugFailClear();
        xrtClearError();
    } else assert(xrtMemDebugFailAfter(count));
}

void clipboardFixtureHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void clipboardFixtureHeapEnd(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("XGE_CLIPBOARD_HEAP alloc=%llu free=%llu realloc=%llu\n",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
    fflush(stdout);
    assert(after.AllocCount - before.AllocCount == 100 && after.FreeCount - before.FreeCount == 100);
    assert(after.LiveCount == before.LiveCount && after.ReallocCount == before.ReallocCount);
}

void clipboardFixtureNativeReadLoop(void)
{
    clipboardFixtureHeapBegin();
    for (int i = 0; i < 100; ++i) {
        char* text = NULL;
        size_t size = 0;
        assert(xgeClipboardGetTextOwned(&text,&size) == XGE_OK && size == 4 && !strcmp(text,"next"));
        xrtFree(text);
    }
    clipboardFixtureHeapEnd();
}

void clipboardFixtureDataHeapEnd(int managed)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    size_t allocations = after.AllocCount - before.AllocCount;
    size_t frees = after.FreeCount - before.FreeCount;
    printf("XGE_CLIPBOARD_DATA_HEAP managed=%d alloc=%llu free=%llu realloc=%llu\n",managed,
        (unsigned long long)allocations,(unsigned long long)frees,
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
    fflush(stdout);
    assert(allocations == frees && after.LiveCount == before.LiveCount);
    assert(after.ReallocCount == before.ReallocCount);
    if (!managed) assert(allocations == 100);
}

void clipboardFixtureNativeDataLoop(void)
{
    static const unsigned char expected[] = {0x11,0,0x22,0xff};
    clipboardFixtureHeapBegin();
    for (int i = 0; i < 100; ++i) {
        void* data = NULL;
        size_t size = 0;
        assert(xgeClipboardGetDataOwned("application/x-xlang-clipboard-test",&data,&size) == XGE_OK);
        assert(size == sizeof(expected) && !memcmp(data,expected,size));
        xrtFree(data);
    }
    clipboardFixtureDataHeapEnd(0);
}
