#include "foreign_xui_callback_resource_outputs.h"

static xvalue* callbackState;
static xvalueviewv1 reader;

int xl_test_callback_output_state(xvalue** output)
{
    *output = NULL;
    if (callbackState) return XUI_ERROR_INVALID_STATE;
    callbackState = xrtValueObject();
    if (!callbackState) return XUI_ERROR_OUT_OF_MEMORY;
    *output = xrtValueRetain(callbackState);
    return *output ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

int xl_test_callback_output_read_begin(void)
{return xrtValueViewBeginV1(callbackState, &reader) ? XUI_OK : XUI_ERROR_INVALID_STATE;}

int xl_test_callback_output_read_end(void)
{
    if (!xrtValueViewEndV1(&reader)) return XUI_ERROR_INVALID_STATE;
    xrtValueRelease(callbackState);
    callbackState = NULL;
    return XUI_OK;
}

int xl_test_callback_output_native_failure(xui_proxy proxy)
{
    const xui_surface_desc_t desc = {.iWidth = 4, .iHeight = 3};
    xui_surface surface = (xui_surface)(uintptr_t)1;
    const int status = proxy->surfaceCreate(proxy, &surface, &desc);
    return status == XUI_ERROR_CALLBACK_FAILED && !surface ? XUI_OK : XUI_ERROR_INVALID_STATE;
}

static bool addEdge(xrtownershipref edge, void* user)
{
    if (!edge.Data || !edge.Ops) return false;
    ++*(int*)user;
    return true;
}

int xl_test_callback_output_proxy_edges(xui_proxy proxy)
{
    xrtownershipscope freeze = {0};
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XUI_ERROR_BUSY;
    const xrtownershipref node = xuiProxyOwnership(proxy);
    int edges = 0;
    if (!node.Ops->Trace(node.Data, addEdge, &edges)) edges = XUI_ERROR_INVALID_STATE;
    return xrtOwnershipScopeEnd(&freeze) ? edges : XUI_ERROR_INVALID_STATE;
}
