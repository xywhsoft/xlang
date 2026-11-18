#include "foreign_xge_nine_patch.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureNinePatchLegacy(xge_texture texture, const xge_nine_patch_style_t* style, xge_rect_t dst)
{
    xge_nine_patch_t patch;
    xgeNinePatchInit(&patch, texture, style->tSource, style->fX1, style->fY1, style->fX2, style->fY2);
    xgeNinePatchSetMode(&patch, style->iMode);
    xgeNinePatchSetColor(&patch, style->iColor);
    xgeNinePatchDraw(&patch, dst, XGE_DRAW_SCREEN_SPACE);
}

void fixtureNinePatchHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void fixtureNinePatchHeapEnd(int mode)
{
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
    printf("NINE_PATCH_HEAP mode=%d alloc=0 free=0 realloc=0\n", mode);
}

void fixtureNinePatchNativeHot(xge_texture texture, const xge_nine_patch_style_t* style, xge_rect_t dst)
{
    fixtureNinePatchHeapBegin();
    for (int i = 0; i < 20; ++i)
        assert(xgeTextureDrawNinePatch(texture, style, dst, XGE_DRAW_SCREEN_SPACE) == XGE_OK);
    fixtureNinePatchHeapEnd(1);
}
