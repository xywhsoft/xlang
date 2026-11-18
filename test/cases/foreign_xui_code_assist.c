#include "foreign_xui_code_assist.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void codeAssistHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void codeAssistHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XUI_CODE_ASSIST_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void codeAssistHeapBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

void codeAssistNativeHot(xui_widget editor, uint32_t version, int offset)
{
    codeAssistHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        uint32_t observedVersion;
        int observedOffset;
        assert(xuiCodeEditGetCompletionPosition(editor, &observedVersion, &observedOffset) == XUI_OK);
        assert(observedVersion == version && observedOffset == offset);
    }
    codeAssistHeapEnd(1);
}
