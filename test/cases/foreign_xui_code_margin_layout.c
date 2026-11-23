#define XRT_MODULE_ALL
#include "foreign_xui_code_margin_layout.h"
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureMarginLayoutBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureMarginLayoutReport(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("MARGIN_LAYOUT_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

void fixtureMarginLayoutBudget(int budget)
{
    if (budget < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(budget));
}

void fixtureMarginLayoutProbe(xui_code_margin_model model, xui_rect_t viewport)
{
    int expected_total;
    xui_rect_t expected_text, first;
    assert(xuiCodeMarginModelMeasureLayout(model, viewport, &expected_total, &expected_text) == XUI_OK);
    assert(xuiCodeMarginModelGetRect(model, 10, viewport, &first) == XUI_OK);
    assert(expected_total == 2);

    xui_code_margin_layout_t* failed = NULL;
    const xui_rect_t invalid = {0, 0, -1, 0};
    assert(xuiCodeMarginModelCaptureLayout(model, invalid, -1, &failed) == XUI_ERROR_INVALID_ARGUMENT && !failed);
    assert(xuiCodeMarginModelCaptureLayout(model, viewport, -2, &failed) == XUI_ERROR_INVALID_ARGUMENT && !failed);
    fixtureMarginLayoutBegin();
    fixtureMarginLayoutBudget(0);
    assert(xuiCodeMarginModelCaptureLayout(model, viewport, -1, &failed) == XUI_ERROR_OUT_OF_MEMORY && !failed);
    fixtureMarginLayoutBudget(-1);
    xrtClearError();

    fixtureMarginLayoutBegin();
    for (int iteration = 0; iteration < 1000; ++iteration) {
        xui_code_margin_layout_t* layout = NULL;
        assert(xuiCodeMarginModelCaptureLayout(model, viewport, -1, &layout) == XUI_OK);
        assert(xuiCodeMarginLayoutAddRef(layout) == XUI_OK);
        int total;
        xui_rect_t text;
        assert(xuiCodeMarginLayoutGetState(layout, &total, &text) == XUI_OK);
        assert(total == expected_total && text.fX == expected_text.fX && text.fW == expected_text.fW);
        const xui_code_margin_geometry_t* entries = NULL;
        size_t count = 0;
        assert(xuiCodeMarginLayoutGetEntries(layout, &entries, &count) == XUI_OK && count == 2);
        xuiCodeMarginLayoutRelease(layout);
        assert(entries[0].tState.iId == 10 && entries[0].tRect.fX == first.fX && entries[0].tRect.fW == first.fW);
        assert(entries[1].tState.iId == 30 && entries[1].tRect.fX == first.fX+first.fW);
        xuiCodeMarginLayoutRelease(layout);
    }
    fixtureMarginLayoutReport(0);
}
