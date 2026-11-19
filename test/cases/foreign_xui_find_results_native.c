#include "foreign_xui_find_results.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;

void findResultsHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void findResultsHeapEnd(int expected)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("XUI_FIND_RESULT_HEAP alloc=%llu free=%llu realloc=%llu\n",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
    fflush(stdout);
    assert(after.AllocCount - before.AllocCount == (size_t)expected);
    assert(after.FreeCount - before.FreeCount == (size_t)expected);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
}

int findResultsTextEqual(xstrview left, xstrview right)
{
    return left.Size == right.Size && (!left.Size || memcmp(left.Data, right.Data, left.Size) == 0);
}

void findResultsNativeHot(xui_widget editor, xui_code_find_scope scope)
{
    findResultsHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xui_code_find_result_t local, shared;
        assert(xuiCodeEditGetFindResult(editor, 0, &local) == XUI_OK);
        assert(xuiCodeFindScopeGetResult(scope, 0, &shared) == XUI_OK);
        assert(local.iStart == 6 && local.iEnd == 11 && local.iLine == 0 && local.iColumn == 4);
        assert(local.iStart == shared.iStart && local.iEnd == shared.iEnd);
        assert(strcmp(local.sPreview, shared.sPreview) == 0);
    }
    findResultsHeapEnd(0);
}
