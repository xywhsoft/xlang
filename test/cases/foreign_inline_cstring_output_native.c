#include "foreign_inline_cstring_output.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

static xmemdebugsnapshot before;

int inlineTextRead(int mode, InlineTextOutput* result)
{
    assert(result && result->iSize == sizeof(*result));
    result->number = 27;
    result->borrowed = mode == 1 ? NULL : mode == 4 ? "" : "pointer";
    memcpy(result->suffix, "suffix", sizeof("suffix"));
    if (mode >= 2 && mode <= 3) {
        memset(result->preview, 'x', sizeof(result->preview));
        return mode == 3 ? -3 : 0;
    }
    memcpy(result->preview, mode == 1 ? "\0\0\0\0\0\0\0" : "\xe4\xbd\xa0\xf0\x9f\x98\x80", sizeof(result->preview));
    return 0;
}

void inlineTextBudget(int count)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(count));
}

void inlineTextReset(void)
{
    xrtMemDebugFailClear();
    xrtClearError();
}

void inlineTextHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void inlineTextHeapEqual(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
}
