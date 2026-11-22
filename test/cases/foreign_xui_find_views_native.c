#include "foreign_xui_find_views.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void findViewsHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void findViewsHeapEnd(int expected)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("XUI_FIND_VIEWS_HEAP alloc=%llu free=%llu realloc=%llu\n",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
    fflush(stdout);
    assert(after.AllocCount - before.AllocCount == (size_t)expected);
    assert(after.FreeCount - before.FreeCount == (size_t)expected);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
}

void findViewsNativeScopeHot(xui_code_find_scope scope, xui_context context, xui_widget editor)
{
    findViewsHeapBegin();
    for (int i = 0; i < 1000; ++i)
        assert(xuiContextGetCodeFindScope(context, editor) == scope);
    findViewsHeapEnd(0);
}

int findViewsSetRawScope(xui_widget editor, int attached)
{
    if (!attached) return xuiCodeFindScopeDestroyChecked(xuiCodeEditGetFindScope(editor));
    xui_code_find_scope scope = NULL;
    int status = xuiCodeFindScopeCreate(&scope);
    if (status != XUI_OK) return status;
    status = xuiCodeEditSetFindScope(editor, scope);
    if (status != XUI_OK) xuiCodeFindScopeDestroy(scope);
    return status;
}

void findViewsNativeHot(xui_code_find_scope scope, xui_context context,
    xui_widget editor, xui_widget window)
{
    findViewsHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xui_widget fromIndex, fromActive, fromResult, fromWindow;
        assert(xuiCodeFindScopeGetEditorChecked(scope, context, 0, &fromIndex) == XUI_OK && fromIndex == editor);
        assert(xuiCodeFindScopeGetActiveEditorChecked(scope, context, &fromActive) == XUI_OK && fromActive == editor);
        assert(xuiCodeFindScopeGetResultEditorChecked(scope, context, 0, &fromResult) == XUI_OK && fromResult == editor);
        assert(xuiCodeFindScopeGetFindWindowChecked(scope, context, &fromWindow) == XUI_OK && fromWindow == window);
    }
    findViewsHeapEnd(0);
}
