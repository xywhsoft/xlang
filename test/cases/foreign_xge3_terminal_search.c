#include "foreign_xge3_terminal_search.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void terminalSearchHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void terminalSearchHeapEnd(int mode)
{
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount && before.LiveCount == after.LiveCount);
    printf("TERMINAL_MATCH_HEAP mode=%d alloc=0 free=0 realloc=0\n", mode);
}

void terminalSearchNativeHot(xui_widget terminal)
{
    terminalSearchHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        int line, column, length;
        assert(xuiTerminalGetFindMatch(terminal, &line, &column, &length) == 1);
        assert(line == 0 && column == 3 && length == 3);
    }
    terminalSearchHeapEnd(1);
}

void terminalSearchNativeOom(xui_widget terminal)
{
    int failures = 0;
    for (int budget = 0; budget != 32; ++budget) {
        int line, column, length;
        assert(xuiTerminalFindText(terminal, "foo", XUI_TERMINAL_SEARCH_WHOLE_WORD, &line, &column) == 1);
        assert(line == 0 && column == 3);
        line = -13;
        column = -17;
        assert(xrtMemDebugFailAfter((size_t)budget));
        int status = xuiTerminalFindText(terminal, "gap", 0, &line, &column);
        int triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (!triggered) {
            assert(status == 1 && line == 0 && column == 7 && failures > 0);
            printf("TERMINAL_SEARCH_OOM_OK failures=%d\n", failures);
            return;
        }
        ++failures;
        xrtClearError();
        assert(status == XUI_ERROR_OUT_OF_MEMORY && line == -13 && column == -17);
        assert(xuiTerminalGetFindMatch(terminal, &line, &column, &length) == 1);
        assert(line == 0 && column == 3 && length == 3);
    }
    assert(!"allocation sweep never reached success");
}
