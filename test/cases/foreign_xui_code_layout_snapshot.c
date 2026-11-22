#undef NDEBUG
#include "test/cases/foreign_xui_code_layout_snapshot.h"
#include "test/cases/foreign_xui_render_backend.c"

static xmemdebugsnapshot before;

void codeLayoutHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void codeLayoutCheckHeap(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount);
    assert(after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount);
}

void codeLayoutCheckNative(void)
{
    xui_code_document document = NULL;
    assert(xuiCodeDocumentCreate(&document) == XUI_OK);
    char text[97];
    for (int row = 0; row < 48; ++row) {
        text[row * 2] = 'a';
        text[row * 2 + 1] = '\n';
    }
    text[96] = '\0';
    assert(xuiCodeDocumentSetText(document, text) == XUI_OK);
    xui_code_layout_desc_t desc = {0};
    desc.iSize = sizeof(desc);
    desc.pDocument = document;
    desc.tViewportRect = (xui_rect_t){10, 20, 200, 48 * 16};
    desc.fLineHeight = 16;
    desc.fCharWidth = 8;
    desc.fMarginWidth = 40;
    xui_code_layout_line_t expected[48];
    xui_vec2_t size;
    xui_rect_t rect;
    int expectedCount;
    assert(xuiCodeLayoutBuildVisibleLines(&desc, expected, 48, &expectedCount, &size, &rect) == XUI_OK);
    assert(expectedCount == 48);
    int failures = 0;
    for (int allowance = 0; allowance < 8; ++allowance) {
        xmemdebugsnapshot start, end;
        assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
        xrtMemDebugSnapshot(&start);
        xui_code_layout_snapshot snapshot = NULL;
        fixtureRenderBudget(allowance);
        int status = xuiCodeLayoutCapture(&desc, &snapshot);
        fixtureRenderBudget(-1);
        xrtErrorFree(xrtTakeError());
        if (status == XUI_ERROR_OUT_OF_MEMORY) {
            assert(snapshot == NULL);
            ++failures;
        } else {
            const xui_code_layout_line_t* lines = NULL;
            size_t count = 0;
            xui_vec2_t actualSize;
            xui_rect_t actualRect;
            assert(status == XUI_OK && snapshot != NULL);
            assert(xuiCodeLayoutSnapshotGetLines(snapshot, &lines, &count) == XUI_OK);
            assert(count == (size_t)expectedCount && memcmp(lines, expected, sizeof(expected)) == 0);
            assert(xuiCodeLayoutSnapshotGetGeometry(snapshot, &actualSize, &actualRect) == XUI_OK);
            assert(memcmp(&size, &actualSize, sizeof(size)) == 0 && memcmp(&rect, &actualRect, sizeof(rect)) == 0);
            xuiCodeLayoutSnapshotRelease(snapshot);
        }
        xrtMemDebugSnapshot(&end);
        assert(start.LiveCount == end.LiveCount && start.LiveBytes == end.LiveBytes);
    }
    assert(failures > 2 && failures < 8);
    assert(xuiCodeDocumentDestroyChecked(document) == XUI_OK);
}
