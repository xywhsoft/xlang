#include "foreign_proxy_resource_value.h"
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { fprintf(stderr, "resource value check failed at line %d\n", __LINE__); return XUI_ERROR_INVALID_STATE; } \
} while (0)

typedef struct ResourceEdges {
    xrtownershipref expected[3];
    unsigned count;
    unsigned seen;
} ResourceEdges;

static bool visitEdge(xrtownershipref edge, void* data)
{
    ResourceEdges* edges = data;
    for (unsigned i = 0; i < edges->count; ++i) {
        if (edge.Data == edges->expected[i].Data && edge.Ops == edges->expected[i].Ops) {
            if (edges->seen & (1u << i)) return false;
            edges->seen |= 1u << i;
            return true;
        }
    }
    return false;
}

static bool traceExact(xrtownershipref root, ResourceEdges edges)
{
    xrtownershipscope freeze = {0};
    if (!root.Data || !xrtOwnershipFreezeTryBegin(&freeze)) return false;
    const bool traced = root.Ops->Trace(root.Data, visitEdge, &edges);
    return xrtOwnershipScopeEnd(&freeze) && traced && edges.seen == (1u << edges.count) - 1u;
}

static int32 wrongThread(void* payload)
{
    xui_font font = payload;
    xvalue* output = NULL;
    bool consumed = false;
    CHECK(xuiFontGetValueOwned(font, &output) == XUI_ERROR_BUSY && !output);
    CHECK(xuiFontReleaseOwnerChecked(font, &consumed) == XUI_ERROR_BUSY && !consumed);
    return XUI_OK;
}

static bool holdOwner(const void* value)
{
    return xrtValueRetain(value) != NULL;
}

static bool dropOwner(const void* value)
{
    xrtValueRelease((xvalue*)value);
    return xrtGetError() == NULL;
}

static int plainFinalize(xui_proxy proxy, void* state)
{
    (void)proxy;
    (void)state;
    return XUI_OK;
}

static int checkValues(xui_proxy proxy)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    xvalue* value = xrtValueObject();
    CHECK(value);
    xui_surface surface = NULL;
    xui_font font = NULL;
    xui_draw_context draw = NULL;
    CHECK(xuiSurfaceCreateWithValueOwned(proxy, value, &surface) == XUI_OK && surface);
    CHECK(xuiFontCreateWithValueOwned(proxy, value, &font) == XUI_OK && font);
    CHECK(xuiDrawCreateWithValueOwned(proxy, value, surface, &draw) == XUI_OK && draw);

    const xrtownershipref proxyEdge = xuiProxyOwnership(proxy);
    const xrtownershipref valueEdge = xrtValueOwnership(value);
    const xrtownershipref surfaceEdge = xuiSurfaceOwnership(surface);
    CHECK(traceExact(surfaceEdge, (ResourceEdges){.expected = {proxyEdge, valueEdge}, .count = 2}));
    CHECK(traceExact(xuiFontOwnership(font), (ResourceEdges){.expected = {proxyEdge, valueEdge}, .count = 2}));
    CHECK(traceExact(xuiDrawOwnership(draw), (ResourceEdges){.expected = {proxyEdge, valueEdge, surfaceEdge}, .count = 3}));
    xvalue* taken = NULL;
    CHECK(xuiSurfaceGetValueOwned(surface, &taken) == XUI_OK && taken == value);
    xrtValueRelease(taken);
    CHECK(xuiFontGetValueOwned(font, &taken) == XUI_OK && taken == value);
    xrtValueRelease(taken);
    CHECK(xuiDrawGetValueOwned(draw, &taken) == XUI_OK && taken == value);
    xrtValueRelease(taken);

    xui_surface separate = NULL;
    CHECK(xuiSurfaceCreateWithValueOwned(proxy, value, &separate) == XUI_OK);
    xvalueviewv1 view = {0};
    CHECK(xrtValueViewBeginV1(value, &view));
    const int closed = xuiDrawEndChecked(draw);
    const bool active = !xuiDrawEnded(draw);
    bool consumedSurface = true, consumedFont = true, consumedDraw = true;
    const int surfaceRelease = xuiSurfaceReleaseOwnerChecked(separate, &consumedSurface);
    const int fontRelease = xuiFontReleaseOwnerChecked(font, &consumedFont);
    const int drawRelease = xuiDrawReleaseOwnerChecked(draw, &consumedDraw);
    const bool untied = xrtValueViewEndV1(&view);
    CHECK(surfaceRelease == XUI_ERROR_BUSY && !consumedSurface);
    CHECK(fontRelease == XUI_ERROR_BUSY && !consumedFont);
    CHECK(drawRelease == XUI_ERROR_BUSY && !consumedDraw);
    CHECK(xuiSurfaceReleaseChecked(separate) == XUI_OK);
    CHECK(closed == XUI_ERROR_BUSY && active && untied);
    const xui_proxy_t desc = {.iSize = sizeof(desc), .iVersion = XUI_PROXY_VERSION};
    const xui_callback_owner_t owner = {.tReference = valueEdge, .onHold = holdOwner, .onDrop = dropOwner};
    xui_proxy other = NULL;
    CHECK(xuiProxyCreateOwnedEx(&desc, &owner, XUI_PROXY_OWNED_ANYTHREAD_FINALIZE, &other) == XUI_OK);
    xui_draw_context foreignTarget = draw;
    CHECK(xuiDrawCreateWithValueOwned(other, value, surface, &foreignTarget) == XUI_ERROR_INVALID_ARGUMENT && !foreignTarget);
    xui_font otherFont = NULL;
    CHECK(xuiFontCreateWithValueOwned(other, value, &otherFont) == XUI_OK);
    xthread* worker = xrtThreadCreate(wrongThread, otherFont, 0);
    CHECK(worker);
    const int joined = xrtThreadWait(worker);
    const int code = xrtThreadExitCode(worker);
    xrtThreadDestroy(worker);
    CHECK(joined == XWAIT_OK && code == XUI_OK);
    CHECK(xuiFontReleaseChecked(otherFont) == XUI_OK);
    CHECK(xuiProxyReleaseChecked(other) == XUI_OK);
    CHECK(xuiDrawEndChecked(draw) == XUI_OK && xuiDrawEnded(draw));
    CHECK(xuiDrawGetValueOwned(draw, &taken) == XUI_ERROR_INVALID_STATE && !taken);
    CHECK(xuiDrawReleaseChecked(draw) == XUI_OK);

    for (int kind = 0; kind < 3; ++kind) {
        int failure;
        bool blank;
        CHECK(xrtMemDebugFailAfter(0));
        if (kind == 0) {
            xui_surface rejected = surface;
            failure = xuiSurfaceCreateWithValueOwned(proxy, value, &rejected);
            blank = rejected == NULL;
        } else if (kind == 1) {
            xui_font rejected = font;
            failure = xuiFontCreateWithValueOwned(proxy, value, &rejected);
            blank = rejected == NULL;
        } else {
            xui_draw_context rejected = draw;
            failure = xuiDrawCreateWithValueOwned(proxy, value, surface, &rejected);
            blank = rejected == NULL;
        }
        const bool hit = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(failure == XUI_ERROR_OUT_OF_MEMORY && blank && hit);
    }
    CHECK(xuiFontReleaseChecked(font) == XUI_OK);
    CHECK(xuiSurfaceReleaseChecked(surface) == XUI_OK);
    xui_surface plain = NULL;
    void* payload = NULL;
    CHECK(xuiSurfaceCreateOwned(proxy, 0, plainFinalize, &plain, &payload) == XUI_OK);
    CHECK(xuiSurfaceGetValueOwned(plain, &taken) == XUI_ERROR_UNSUPPORTED && !taken);
    CHECK(xuiSurfaceReleaseChecked(plain) == XUI_OK);
    xrtValueRelease(value);
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    return XUI_OK;
}

int xl_test_proxy_resource_value(xui_proxy proxy)
{
    const bool enabled = xrtMemDebugEnabled();
    if (!enabled && !xrtMemDebugEnable(true)) return XUI_ERROR_INVALID_STATE;
    const int result = checkValues(proxy);
    if (!enabled) (void)xrtMemDebugEnable(false);
    return result;
}
