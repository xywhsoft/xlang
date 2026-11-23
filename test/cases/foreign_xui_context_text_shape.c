#include "test/cases/foreign_xui_context_text_shape.h"
#include "test/cases/foreign_xui_render_backend.c"

static int capsCalls;
static bool blockCaps;

static int checkedCaps(xui_proxy proxy, xui_proxy_caps_t* output)
{
    ++capsCalls;
    if (blockCaps) return XUI_ERROR_UNSUPPORTED;
    return backend.tProxy.getCaps(proxy, output);
}

static int checkedShape(xui_proxy proxy, const xui_text_item_t* request, xui_text_shape_t* output)
{
    xui_text_item_t item = *request;
    item.pFont = unwrap(item.pFont, FONT);
    return backend.tProxy.textShape(proxy, &item, output);
}

static void installShapeProbe(xui_proxy proxy)
{
    proxy->getCaps = checkedCaps;
    proxy->textShape = checkedShape;
}

int fixtureShapeContextProxy(xui_proxy* output)
{
    capsCalls = 0;
    blockCaps = false;
    int status = fixtureRenderProxy(output);
    if (status == XUI_OK) installShapeProbe(*output);
    return status;
}

int fixtureShapeContextOtherProxy(xui_proxy* output)
{
    int status = fixtureRenderSecondaryProxy(output);
    if (status == XUI_OK) installShapeProbe(*output);
    return status;
}

void fixtureShapeContextBlockCaps(void) { blockCaps = true; }
int fixtureShapeContextCapsCalls(void) { return capsCalls; }

void fixtureShapeContextCompare(xui_context context, xui_font font)
{
    static const char content[] = "a\0\xe6\x96\x87";
    const xui_text_item_t item = {
        .iSize = sizeof(item), .pFont = font, .sText = content,
        .iTextSize = sizeof(content) - 1, .iFlags = 11
    };
    xui_text_shape_t raw = {0};
    const xui_text_shape_t* owned = NULL;
    assert(xuiTextShape(context, &item, &raw) == XUI_OK);
    assert(xuiContextTextShapeCreateOwned(context, &item, &owned) == XUI_OK);
    assert(raw.iTextSize == owned->iTextSize && raw.fWidth == owned->fWidth);
    assert(raw.iClusterCount == 3 && raw.iClusterCount == owned->iClusterCount);
    for (int index = 0; index < raw.iClusterCount; ++index) {
        assert(raw.pClusters[index].iTextStart == owned->pClusters[index].iTextStart);
        assert(raw.pClusters[index].iTextEnd == owned->pClusters[index].iTextEnd);
    }
    assert(xuiTextShapeFreeChecked(&raw) == XUI_OK);
    assert(xuiTextShapeReleaseChecked(owned) == XUI_OK);
}
