#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include <xui.h>
#include <xge.h>
#include <xge_host_xrt.h>
#include <xge_host_xrt_contract.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct HostSymbol { const char* name; xge_host_xrt_proc value; } HostSymbol;
static HostSymbol symbols[]={
#define XGE_HOST_XRT_SLOT(name) {#name,NULL},
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
};
static xge_host_xrt_proc resolve(void* user,const char* name)
{
    (void)user; size_t low=0,high=sizeof(symbols)/sizeof(symbols[0]);
    while (low<high) {
        size_t middle=low+(high-low)/2; int order=strcmp(symbols[middle].name,name);
        if (order<0) low=middle+1;
        else if (order>0) high=middle;
        else return symbols[middle].value;
    }
    return NULL;
}
static bool watch_context(void* raw,void* storage,void (*notify)(void*),void* user)
{ return xuiContextLifetimeWatch((xui_context)raw,storage,notify,user)==XUI_OK; }
static bool watch_widget(void* raw,void* storage,void (*notify)(void*),void* user)
{ return xuiWidgetLifetimeWatch((xui_widget)raw,storage,notify,user)==XUI_OK; }
static void unwatch(void* storage) { xuiLifetimeUnwatch(storage); }
static bool destroy_context(void* raw) { xuiDestroy(raw); return true; }
static bool destroy_widget(void* raw) { xuiWidgetDestroy(raw); return true; }
static bool native_begin(void* root,void* storage) { return xuiCallScopeBegin(root,storage)==XUI_OK; }
static bool native_end(void* storage) { return xuiCallScopeEnd(storage)==XUI_OK; }
static bool native_busy(const void* root) { return xuiContextOwnershipBusy((xui_context)root); }
/* This CPU fixture creates only base widgets, with no value models or retained
 * language callbacks. NULL TraceRaw is NOT a general XUI context leaf policy. */
static const XL6_ForeignViewDescriptorV1 contextDescriptor={
    sizeof(contextDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"xui_context",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_context,unwatch,destroy_context,
    sizeof(xui_call_scope_t),_Alignof(xui_call_scope_t),native_begin,native_end,NULL,native_busy};
static const XL6_ForeignViewDescriptorV1 widgetDescriptor={
    sizeof(widgetDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"xui_widget",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_widget,unwatch,destroy_widget,0,1,NULL,NULL,NULL,NULL};

static void real_context_lifetime(void)
{
    xui_context raw=NULL; xui_widget a=NULL,b=NULL;
    assert(xuiCreate(&raw)==XUI_OK);
    XL6_ForeignView* context=xl6ForeignViewCreateRootV1(raw,&contextDescriptor,NULL); assert(context);
    xui_call_scope_t native={0}; XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(context,&call,&native,sizeof(native)));
    assert(xuiWidgetCreate(raw,&a)==XUI_OK && xuiWidgetCreate(raw,&b)==XUI_OK);
    XL6_ForeignView* first=xl6ForeignViewBorrowChildV1(a,context,&widgetDescriptor,NULL); assert(first);
    XL6_ForeignView* second=xl6ForeignViewBorrowChildV1(b,context,&widgetDescriptor,NULL); assert(second);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    XL6_ForeignView* same=xl6ForeignViewBorrowChildV1(a,context,&widgetDescriptor,NULL); assert(same==first);
    assert(xl6ForeignViewReleaseV1(same));
    assert(xrtMemDebugFailAfter(-1)); xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    xuiWidgetDestroy(a); /* Native-side close, outside the language controller. */
    XL6_ForeignViewCallV1 rejected={0}; xui_call_scope_t rejectedNative={0};
    assert(!xl6ForeignViewCallBeginV1(first,&rejected,&rejectedNative,sizeof(rejectedNative))); xrtClearError();
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewCallBeginV1(second,&call,&native,sizeof(native)) && call.raw==b);
    assert(xuiWidgetGetChildCount((xui_widget)call.raw)==0);
    assert(xl6ForeignViewCallEndV1(&call));
    /* Reuse native allocator slots without reviving an old language value. */
    for (unsigned i=0;i<64;++i) {
        xui_widget next=NULL;
        assert(xl6ForeignViewCallBeginV1(context,&call,&native,sizeof(native)));
        assert(xuiWidgetCreate(raw,&next)==XUI_OK);
        XL6_ForeignView* value=xl6ForeignViewBorrowChildV1(next,context,&widgetDescriptor,NULL); assert(value && value!=first);
        assert(xl6ForeignViewCloseV1(value));
        assert(xl6ForeignViewCallEndV1(&call)); assert(xl6ForeignViewReleaseV1(value));
    }
    assert(xl6ForeignViewCallBeginV1(context,&call,&native,sizeof(native)));
    xuiDestroy(raw); /* Logical invalidation now; physical free at CallEnd. */
    assert(!xl6ForeignViewCallBeginV1(second,&rejected,&rejectedNative,sizeof(rejectedNative))); xrtClearError();
    assert(xl6ForeignViewCloseV1(second)); /* No rollback through a stale pointer. */
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(first) && xl6ForeignViewReleaseV1(second));
    assert(xl6ForeignViewReleaseV1(context));
}
int main(void)
{
    size_t index=0;
#define XGE_HOST_XRT_SLOT(name) symbols[index++].value=(xge_host_xrt_proc)(name);
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
    assert(index==sizeof(symbols)/sizeof(symbols[0]));
    const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
    assert(xgeHostXrtRuntimeIdentity()==(xge_host_xrt_proc)xrtVersion);
    xge_build_info_t build={0}; build.size=sizeof(build);
    assert(xgeGetBuildInfo(&build)==XGE_OK && build.abi_fingerprint==XGE_BUILD_ABI_FINGERPRINT);
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    real_context_lifetime(); xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(xl6ForeignViewPendingV1(0)==0);
    puts("real hosted XUI: canonical views, native close, allocator reuse, context invalidation and cleanup passed");
    return 0;
}
