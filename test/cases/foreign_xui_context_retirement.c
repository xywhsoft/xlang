#include "foreign_xui_context_retirement.h"

static struct {
    xui_proxy proxy;
    xvalue* value;
    xvalueviewv1 read;
    xui_lifetime_link_t terminal;
    size_t ownerRefs;
    int state;
} fixture;

static bool ownerCount(const void* owner, size_t* output)
{
    if (owner != &fixture) return false;
    *output = fixture.ownerRefs;
    return true;
}

static bool ownerTrace(const void* owner, xrtownershipvisitor visit, void* user)
{
    return owner == &fixture && visit(xrtValueOwnership(fixture.value), user);
}

static bool ownerHold(const void* owner)
{
    if (owner != &fixture) return false;
    ++fixture.ownerRefs;
    return true;
}

static bool ownerDrop(const void* owner)
{
    if (owner != &fixture || fixture.state || fixture.ownerRefs != 1) return false;
    --fixture.ownerRefs;
    xui_surface output = NULL;
    if (xuiSurfaceStageCallbackOutput(&output, fixture.proxy, fixture.value) != XUI_OK)
        return false;
    if (!xrtValueViewBeginV1(fixture.value, &fixture.read)) {
        xuiSurfaceRollbackCallbackOutput(output);
        return false;
    }
    xuiSurfaceRollbackCallbackOutput(output);
    fixture.state = 1;
    return true;
}

static void onError(xui_context context, const xui_error_info_t* error, void* owner)
{
    (void)context;
    (void)error;
    (void)owner;
}

static void onRetired(void* owner)
{
    if (owner == &fixture && fixture.state == 1 && !fixture.value)
        fixture.state = 2;
    else
        fixture.state = -1;
}

int fixtureInstallLateContextOutput(xui_context context)
{
    static const xrtownershipops ops = {ownerCount, ownerTrace};
    const xui_callback_owner_t owner = {{&fixture, &ops}, ownerHold, ownerDrop};
    fixture.proxy = xuiGetProxyOwned(context);
    if (!fixture.proxy) return XUI_ERROR_INVALID_STATE;
    fixture.value = xrtValueObject();
    if (!fixture.value) return XUI_ERROR_OUT_OF_MEMORY;
    int status = xuiContextRetirementWatch(context, &fixture.terminal, onRetired, &fixture);
    if (status == XUI_OK) status = xuiSetErrorCallbackOwned(context, onError, &fixture, &owner);
    if (status != XUI_OK) {
        xuiLifetimeUnwatch(&fixture.terminal);
        xrtValueRelease(fixture.value);
        fixture.value = NULL;
    }
    return status;
}

int fixtureFinishLateContextRead(void)
{
    if (fixture.state != 1 || !xrtValueViewEndV1(&fixture.read))
        return XUI_ERROR_INVALID_STATE;
    xrtValueRelease(fixture.value);
    fixture.value = NULL;
    return XUI_OK;
}

int fixtureLateContextState(void)
{
    return fixture.state;
}
