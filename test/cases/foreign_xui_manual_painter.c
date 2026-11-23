#include "foreign_xui_manual_painter.h"
#include "foreign_xui_render_backend.c"

static int visitPainter(xui_painter painter, void* user)
{
    assert(xuiPainterGetDrawContext(painter) == user);
    return XUI_OK;
}

void fixturePainterNativeSample(xui_context context, xui_draw_context draw)
{
    assert(xuiPainterVisit(context, draw, visitPainter, draw) == XUI_OK);
    fixtureRenderHeapBegin();
    for (int i = 0; i < 128; ++i) {
        assert(xuiPainterVisit(context, draw, visitPainter, draw) == XUI_OK);
    }
    fixtureRenderHeapResult(1);
}

int fixtureResourceReserve(xui_resource resource, bool acquire)
{
    return acquire ? xuiResourceAddRef(resource) : xuiResourceRelease(resource);
}
