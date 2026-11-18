#include "test/cases/foreign_xui_message_renderer.h"
#include "test/cases/foreign_xui_render_backend.c"

static const uint32_t messageColor = 0x3B82F6FF;
static int messageFills;
static xmemdebugsnapshot messageHeap;

static int messageFill(xui_proxy proxy, xui_draw_context draw, xui_rect_t rectangle, uint32_t color)
{
    const int result = fixture_drawRectFill(proxy, draw, rectangle, color);
    if (result == XUI_OK && color == messageColor) ++messageFills;
    return result;
}

int messageRendererProxy(xui_proxy* output)
{
    assert(fixtureRenderBalanced());
    xuiTestProxyInit(&backend);
    fixtureRenderResetText();
    messageFills = 0;
    xui_proxy_t table;
    adapt(&table);
    table.drawRectFill = messageFill;
    const xui_callback_owner_t owner = {{&references,&operations},hold,drop};
    return xuiProxyCreateOwned(&table, &owner, output);
}

int messageRendererFillCount(void)
{
    return messageFills;
}

static int nativeMessageRender(xui_widget widget, int index, const xui_message_node_t* node,
    xui_draw_context draw, xui_rect_t rectangle, int state, void* user)
{
    (void)widget;
    (void)index;
    (void)state;
    (void)user;
    if (node->iType != XUI_MESSAGE_NODE_SYSTEM) return 0;
    xui_proxy proxy = xuiDrawGetProxy(draw);
    const int status = proxy->drawRectFill(proxy, draw, rectangle, messageColor);
    return status == XUI_OK ? 1 : status;
}

int messageRendererInstallNative(xui_widget list)
{
    const xui_callback_owner_t owner = {{&references,&operations},hold,drop};
    return xuiMessageListSetNodeRendererOwned(list, nativeMessageRender, NULL, &owner);
}

void messageRendererHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&messageHeap);
}

void messageRendererHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == messageHeap.LiveCount && after.LiveBytes == messageHeap.LiveBytes);
    assert(after.InvalidFreeCount == messageHeap.InvalidFreeCount && after.DoubleFreeCount == messageHeap.DoubleFreeCount);
    printf("XUI_MESSAGE_RENDER_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount-messageHeap.AllocCount),
        (unsigned long long)(after.FreeCount-messageHeap.FreeCount),
        (unsigned long long)(after.ReallocCount-messageHeap.ReallocCount));
}
