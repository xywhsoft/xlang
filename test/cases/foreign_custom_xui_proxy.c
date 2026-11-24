#include "foreign_custom_xui_proxy.h"

typedef struct ProxyOwnerCheck {
    unsigned edges;
} ProxyOwnerCheck;

static bool visitOwner(xrtownershipref owner, void* data)
{
    ProxyOwnerCheck* check = data;
    size_t refs = 0;
    if (!owner.Data || !owner.Ops || !owner.Ops->Count(owner.Data, &refs) || !refs) return false;
    ++check->edges;
    return true;
}

int xl_test_custom_proxy_owner(xui_proxy proxy)
{
    if (!xuiProxyIsOwned(proxy) || proxy->iVersion != XUI_PROXY_VERSION || !proxy->pUser || !proxy->getCaps)
        return XUI_ERROR_INVALID_STATE;
    xrtownershipscope freeze = {0};
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XUI_ERROR_BUSY;
    const xrtownershipref root = xuiProxyOwnership(proxy);
    ProxyOwnerCheck check = {0};
    const bool traced = root.Data && root.Ops->Trace(root.Data, visitOwner, &check);
    return xrtOwnershipScopeEnd(&freeze) && traced && check.edges == 1 ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

int xl_test_custom_proxy_fail_after(int count)
{
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return XUI_ERROR_INVALID_STATE;
    return xrtMemDebugFailAfter(count) ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

bool xl_test_custom_proxy_fail_clear(void)
{
    const bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    return hit;
}
