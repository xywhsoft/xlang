#undef NDEBUG
#include "test/cases/foreign_xui_cascader_path.h"
#include "test/cases/foreign_xui_render_backend.c"

static xmemdebugsnapshot before;

void cascaderHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void cascaderHeapCheck(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount);
    assert(after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount);
}

void cascaderNativeQueries(xui_widget widget)
{
    xui_cascader_item_t item = {0};
    assert(xuiCascaderGetItemDescriptor(widget, 1, &item) == XUI_OK);
    assert(strcmp(item.sText, "Docs") == 0 && item.iValue == 11 && item.iParent == 0);
    const char* text = item.sText;
    assert(xuiCascaderGetItemDescriptor(widget, -1, &item) == XUI_ERROR_INVALID_ARGUMENT);
    assert(item.sText == text && item.iValue == 11 && item.iParent == 0);
    assert(xuiCascaderGetItem(widget, 1)->sText == text);

    int prefix = 0;
    assert(xuiCascaderGetSelectedPath(widget, &prefix, 1) == 2 && prefix == 10);
    xui_cascader_path_t path = {0};
    cascaderHeapBegin();
    for (int n = 0; n < 1000; ++n) {
        assert(xuiCascaderGetSelectedPathSnapshot(widget, &path) == XUI_OK);
        assert(path.iDepth == 2 && path.arrValues[0] == 10 && path.arrValues[1] == 11);
    }
    cascaderHeapCheck();
    assert(xuiCascaderGetSelectedPathSnapshot(NULL, &path) == XUI_ERROR_INVALID_ARGUMENT);
    assert(path.iDepth == 2 && path.arrValues[1] == 11);
}
