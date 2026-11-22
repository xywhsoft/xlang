#include "foreign_xui_table_render_scroll.h"
#include <assert.h>

static xmemdebugsnapshot before;

static void check_snapshot(const xui_scroll_model_t* model, const xui_scroll_model_t* snapshot)
{
    assert(model && snapshot->iSize == sizeof(*model));
    assert(model->tViewportRect.fX == snapshot->tViewportRect.fX);
    assert(model->tViewportRect.fY == snapshot->tViewportRect.fY);
    assert(model->tViewportRect.fW == snapshot->tViewportRect.fW);
    assert(model->tViewportRect.fH == snapshot->tViewportRect.fH);
    assert(model->fContentWidth == snapshot->fContentWidth);
    assert(model->fContentHeight == snapshot->fContentHeight);
    assert(model->fScrollX == snapshot->fScrollX && model->fScrollY == snapshot->fScrollY);
}

void fixtureCompareTableScroll(xui_widget table, xui_widget tree)
{
    xui_scroll_model_t snapshot = {0};
    assert(xuiTableViewGetScrollSnapshot(NULL, &snapshot) == XUI_ERROR_INVALID_ARGUMENT);
    assert(xuiTreeViewGetScrollSnapshot(tree, NULL) == XUI_ERROR_INVALID_ARGUMENT);
    assert(xuiTreeViewGetScrollSnapshot(table, &snapshot) == XUI_ERROR_INVALID_ARGUMENT);
    assert(snapshot.iSize == 0);
    for (int i = 0; i < 1000; i++) {
        assert(xuiTableViewGetScrollSnapshot(table, &snapshot) == XUI_OK);
        check_snapshot(xuiTableViewGetModel(table), &snapshot);
        assert(xuiTreeViewGetScrollSnapshot(tree, &snapshot) == XUI_OK);
        check_snapshot(xuiTreeViewGetModel(tree), &snapshot);
    }
}

void fixtureStartScrollAllocationCheck(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void fixtureFinishScrollAllocationCheck(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount);
    assert(after.ReallocCount == before.ReallocCount);
    assert(after.FreeCount == before.FreeCount);
}
