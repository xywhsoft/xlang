#include "foreign_xui_tabs_close_owned.h"
#include "xrt_decl.h"

#undef NDEBUG
#include <assert.h>

static size_t proxyReferences = 1;
static int rawCloseCount;

static bool holdProxy(const void* data)
{
    assert(data == &proxyReferences);
    ++proxyReferences;
    return true;
}

static bool dropProxy(const void* data)
{
    assert(data == &proxyReferences && proxyReferences > 1);
    --proxyReferences;
    return true;
}

static bool countProxy(const void* data, size_t* count)
{
    assert(data == &proxyReferences);
    *count = proxyReferences;
    return true;
}

static bool traceProxy(const void* data, xrtownershipvisitor visit, void* user)
{
    assert(data == &proxyReferences);
    (void)visit;
    (void)user;
    return true;
}

static int headlessIme(xui_proxy proxy, int enabled)
{
    (void)proxy;
    (void)enabled;
    return XUI_ERROR_UNSUPPORTED;
}

int fixtureTabsCpuProxy(xui_proxy* output)
{
    static const xrtownershipops operations = {countProxy, traceProxy};
    const xui_callback_owner_t owner = {{&proxyReferences, &operations}, holdProxy, dropProxy};
    /* Use native layout and input dispatch without requiring a platform window for IME. */
    xui_proxy_t proxy = xuiProxyXge();
    proxy.imeSetEnabled = headlessIme;
    return xuiProxyCreateOwned(&proxy, &owner, output);
}

static void countRawClose(xui_widget tabs, int index, void* user)
{
    assert(tabs && index == 0);
    ++*(int*)user;
}

int fixtureTabsInstallRawClose(xui_widget tabs)
{
    return xuiTabsSetClose(tabs, countRawClose, 1, &rawCloseCount);
}

int fixtureTabsRawCloseCount(void)
{
    return rawCloseCount;
}

void fixtureTabsArmOom(uint64_t prefix)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(prefix));
}

bool fixtureTabsClearOom(void)
{
    bool triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    return triggered;
}
