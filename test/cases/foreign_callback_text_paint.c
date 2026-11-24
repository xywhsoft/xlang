#include "foreign_callback_text_paint.h"
#include <stdio.h>
#include <string.h>

static int finishFont(xui_proxy proxy, void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_paint_create_shape(xui_proxy proxy, const xui_text_shape_t** shape)
{
    xui_font font = NULL;
    void* payload = NULL;
    int status = xuiFontCreateOwned(proxy, 0, finishFont, &font, &payload);
    if (status != XUI_OK) return status;
    const xui_text_item_t item = {.iSize = sizeof(item), .pFont = font, .sText = "x", .iTextSize = -1};
    status = xuiTextShapeCreateOwned(proxy, &item, shape);
    const int fontStatus = xuiFontReleaseChecked(font);
    return status == XUI_OK ? fontStatus : status;
}

int xl_test_paint_decline_shape(xui_proxy proxy)
{
    const xui_text_shape_t* shape = NULL;
    const int status = xl_test_paint_create_shape(proxy, &shape);
    if (shape) (void)xuiTextShapeReleaseChecked(shape);
    return !shape && status == XUI_ERROR_UNSUPPORTED ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

typedef struct ManagedEdges {
    xrtownershipref proxy;
    xrtownershipref paint;
    unsigned proxyCount;
    unsigned paintCount;
} ManagedEdges;

static bool visitManagedPaint(xrtownershipref ref, void* user)
{
    ManagedEdges* edges = user;
    if (ref.Data == edges->proxy.Data && ref.Ops == edges->proxy.Ops) {
        ++edges->proxyCount;
        return true;
    }
    if (ref.Data == edges->paint.Data && ref.Ops == edges->paint.Ops) {
        ++edges->paintCount;
        return true;
    }
    return false;
}

static bool checkManagedPaintEdge(xui_proxy proxy, const xui_text_shape_t* shape)
{
    xvalue* value = NULL;
    if (xuiTextShapeGetPaintValueOwned(shape, &value) != XUI_OK || !value) return false;
    ManagedEdges edges = {.proxy = xuiProxyOwnership(proxy), .paint = xrtValueOwnership(value)};
    const xrtownershipref owner = xuiTextShapeOwnership(shape);
    bool valid = false;
    xrtownershipscope freeze = {0};
    if (owner.Data && edges.proxy.Data && xrtOwnershipFreezeTryBegin(&freeze)) {
        valid = owner.Ops->Trace(owner.Data, visitManagedPaint, &edges);
        valid = xrtOwnershipScopeEnd(&freeze) && valid;
    }
    xrtValueRelease(value);
    return valid && edges.proxyCount == 1 && edges.paintCount == 1;
}

int xl_test_paint_measure(xui_proxy proxy, const xui_text_shape_t* shape)
{
    if (!checkManagedPaintEdge(proxy, shape)) return XUI_ERROR_INVALID_STATE;
    xui_vec2_t measured = {0};
    const int status = proxy->textShapeRangeMeasure(proxy, shape, 0, shape->iTextSize, &measured);
    if (status != XUI_OK) return status;
    return measured.fX == 17.0f && measured.fY == 2.0f ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

typedef struct PaintProbe {
    xui_text_shape_t* shape;
    xrtownershipref expected;
    unsigned count;
    int readStatus;
    int dropStatus;
} PaintProbe;

static bool visitPaintValue(xrtownershipref target, void* user)
{
    PaintProbe* probe = user;
    ++probe->count;
    return target.Data == probe->expected.Data && target.Ops == probe->expected.Ops;
}

static int32 checkWrongThread(ptr user)
{
    PaintProbe* probe = user;
    xvalue* output = NULL;
    probe->readStatus = xuiTextShapeGetPaintValueOwned(probe->shape, &output);
    xrtValueRelease(output);
    probe->dropStatus = xuiTextShapeFreeChecked(probe->shape);
    return 0;
}

static int checkProtocol(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    xui_text_shape_t shape = {.iSize = sizeof(shape)};
    unsigned char rawBefore[sizeof(shape)];
    memcpy(rawBefore, &shape, sizeof(shape));
    xvalue* value = xrtValueString((xstrview){"paint data", 10});
    if (!value) return XUI_ERROR_OUT_OF_MEMORY;
    xvalue* output = NULL;
    int result = XUI_ERROR_INVALID_STATE;
    const char* step = "empty shape and size validation";

    if (xuiTextShapeGetPaintValueOwned(&shape, &output) != XUI_OK || output ||
        xuiTextShapeAttachPaintValue(&shape, value, SIZE_MAX) != XUI_ERROR_INVALID_ARGUMENT ||
        !xrtMemDebugFailAfter(0)) goto done;
    step = "allocation failure";
    const int failure = xuiTextShapeAttachPaintValue(&shape, value, 0);
    const bool triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    xrtClearError();
    if (!triggered || failure != XUI_ERROR_OUT_OF_MEMORY || memcmp(&shape, rawBefore, sizeof(shape))) goto done;
    step = "one retained attachment";
    if (xuiTextShapeAttachPaintValue(&shape, value, 32) != XUI_OK || shape.iPaintBytes <= 32 ||
        xuiTextShapeAttachPaintValue(&shape, value, 32) != XUI_ERROR_INVALID_STATE) goto done;

    step = "paint graph tracing";
    PaintProbe probe = {.shape = &shape, .expected = xrtValueOwnership(value)};
    xrtownershipscope freeze = {0};
    if (!xrtOwnershipFreezeTryBegin(&freeze)) goto done;
    const bool traced = shape.pPaintOwnership->DropReady(shape.pPaint) &&
        shape.pPaintOwnership->TraceRaw(shape.pPaint, visitPaintValue, &probe);
    const bool ended = xrtOwnershipScopeEnd(&freeze);
    if (!traced || !ended || probe.count != 1) goto done;
    step = "retained value identity";
    if (xuiTextShapeGetPaintValueOwned(&shape, &output) != XUI_OK || output != value) goto done;
    xrtValueRelease(value);
    value = NULL;

    /* A successful thread creation is always joined before accessing the same record. */
    step = "wrong-thread read and retirement";
    xthread* worker = xrtThreadCreate(checkWrongThread, &probe, 0);
    if (!worker) goto done;
    const int waited = xrtThreadWait(worker);
    xrtThreadDestroy(worker);
    if (waited != XWAIT_OK || probe.readStatus != XUI_ERROR_BUSY || probe.dropStatus != XUI_STATUS_DEFERRED || !shape.pPaint) goto done;
    step = "creator-thread retirement";
    if (xuiTextShapeFreeChecked(&shape) != XUI_OK || shape.pPaint || shape.pPaintOwnership) goto done;
    step = "value lifetime after retirement";
    xstrview text = {0};
    if (!xrtValueGetString(output, &text) || text.Size != 10 || memcmp(text.Data, "paint data", 10)) goto done;
    result = XUI_OK;

done:
    if (shape.pPaint) (void)xuiTextShapeFreeChecked(&shape);
    xrtValueRelease(output);
    xrtValueRelease(value);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (result != XUI_OK) fprintf(stderr, "paint protocol check failed at %s\n", step);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount) {
        fprintf(stderr, "paint protocol allocation counters changed\n");
        return XUI_ERROR_INVALID_STATE;
    }
    return result;
}

int xl_test_paint_protocol(void)
{
    const bool enabled = xrtMemDebugEnabled();
    if (!enabled && !xrtMemDebugEnable(true)) return XUI_ERROR_INVALID_STATE;
    const int status = checkProtocol();
    if (!enabled) (void)xrtMemDebugEnable(false);
    return status;
}
