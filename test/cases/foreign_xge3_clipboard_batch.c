#include "test/cases/foreign_xge3_clipboard_batch.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t owner_refs = 1;
static int publish_calls;
static int reject_publish;
static const void* expected_data;

static bool owner_hold(const void* owner)
{
    assert(owner == &owner_refs);
    ++owner_refs;
    return true;
}

static bool owner_drop(const void* owner)
{
    assert(owner == &owner_refs && owner_refs > 1);
    --owner_refs;
    return true;
}

static bool owner_count(const void* owner, size_t* count)
{
    assert(owner == &owner_refs);
    *count = owner_refs;
    return true;
}

static bool owner_trace(const void* owner, xrtownershipvisitor visit, void* user)
{
    assert(owner == &owner_refs);
    (void)visit;
    (void)user;
    return true;
}

static int publish(xui_proxy proxy, const xui_clipboard_item_t* items, int count,
    const xn_transfer_services_v1* services)
{
    assert(proxy && items && (count == 1 || count == 2));
    assert(services && services->iSize == sizeof(*services));
    assert(services->iAbiTag == XN_TRANSFER_SERVICES_V1_TAG && services->pOwnershipDomain);
    assert(services->wrapDataObject && services->wrapDropSource && services->wrapDropTarget);
    assert(services->pCodeCapability || (services->iFlags & XN_TRANSFER_PROCESS_RESIDENT));
    assert(!strcmp(items[0].sFormat, "application/x-xlang-batch-test"));
    assert(items[0].iDataSize == 4 && items[0].pData == expected_data);
    assert(!memcmp(items[0].pData, "A\0B\xff", 4));
    if (count == 2) {
        assert(!strcmp(items[1].sFormat, "application/x-xlang-batch-empty"));
        assert(items[1].iDataSize == 0);
    }
    ++publish_calls;
    return reject_publish ? XUI_ERROR_UNSUPPORTED : XUI_OK;
}

int clipboardBatchProxy(xui_proxy* out)
{
    static const xrtownershipops ops = {owner_count, owner_trace};
    const xui_callback_owner_t owner = {{&owner_refs, &ops}, owner_hold, owner_drop};
    xui_proxy_t proxy = xuiProxyXge();
    proxy.clipboardSetItemsWithTransfer = publish;
    return xuiProxyCreateOwned(&proxy, &owner, out);
}

void clipboardBatchRemember(const void* payload, size_t size)
{
    assert(size == 4);
    expected_data = payload;
}

int clipboardBatchCalls(void) { return publish_calls; }
int clipboardBatchReferences(void) { return (int)owner_refs; }
void clipboardBatchReject(int enabled) { reject_publish = enabled; }
void clipboardBatchFailAfter(int allowance) { assert(xrtMemDebugFailAfter(allowance)); }

int clipboardBatchIsolatedDisplay(void)
{
#if defined(_WIN32)
    return 0;
#else
    const char* enabled = getenv("XLANG_TEST_PRIVATE_CLIPBOARD");
    return enabled && !strcmp(enabled, "1");
#endif
}
