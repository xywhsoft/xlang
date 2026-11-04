/* Policy hooks and host-XRT binding only. XUI operations are called directly
 * by the generated declarations; this file contains no forwarded UI method. */
#include "src/common/xl6_xrt.h"
#include "foreign_xui_declarative.h"
#include <xge.h>
#include <xge_host_xrt.h>
#include <xge_host_xrt_contract.h>
#include <assert.h>
#include <string.h>
_Static_assert(XUI_KEY_ENTER==0x102,"fixture enter key");
_Static_assert(XUI_KEY_ESCAPE==0x103 && XUI_CURSOR_ARROW==1,"fixture public constants");
typedef struct Symbol {const char* name;xge_host_xrt_proc address;} Symbol;
static Symbol symbols[]={
#define XGE_HOST_XRT_SLOT(name) {#name,NULL},
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
};
static xge_host_xrt_proc resolve(void* user,const char* name)
{
    (void)user;size_t first=0,last=sizeof(symbols)/sizeof(*symbols);
    while(first<last){size_t middle=first+(last-first)/2;int order=strcmp(name,symbols[middle].name);
        if(order<0)last=middle;else if(order>0)first=middle+1;else return symbols[middle].address;}
    return NULL;
}
static bool watch_context(void* raw,void* storage,void(*notify)(void*),void* user)
{return xuiContextLifetimeWatch(raw,storage,notify,user)==XUI_OK;}
static bool watch_widget(void* raw,void* storage,void(*notify)(void*),void* user)
{return xuiWidgetLifetimeWatch(raw,storage,notify,user)==XUI_OK;}
static void unwatch(void* storage){xuiLifetimeUnwatch(storage);}
static bool destroy_context(void* raw){xuiDestroy(raw);return true;}
static bool destroy_widget(void* raw){xuiWidgetDestroy(raw);return true;}
static bool begin(void* raw,void* storage){return xuiCallScopeBegin(raw,storage)==XUI_OK;}
static bool end(void* storage){return xuiCallScopeEnd(storage)==XUI_OK;}
static bool busy(const void* raw){return xuiContextOwnershipBusy((xui_context)raw);}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiContextTraceOwnership((xui_context)raw,visit,user);}
const XL6_ForeignViewDescriptorV1 declarativeXuiContextPolicy={sizeof(declarativeXuiContextPolicy),XL6_FOREIGN_VIEW_V1_TAG,"xui_context",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_context,unwatch,destroy_context,
    sizeof(xui_call_scope_t),_Alignof(xui_call_scope_t),begin,end,trace,busy,XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH,NULL,NULL};
const XL6_ForeignViewDescriptorV1 declarativeXuiWidgetPolicy={sizeof(declarativeXuiWidgetPolicy),XL6_FOREIGN_VIEW_V1_TAG,"xui_widget",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_widget,unwatch,destroy_widget,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
void declarative_xui_start(void)
{
    size_t i=0;
#define XGE_HOST_XRT_SLOT(name) symbols[i++].address=(xge_host_xrt_proc)(name);
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
    const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
    assert(xgeHostXrtRuntimeIdentity()==(xge_host_xrt_proc)xrtVersion);
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
}
static xmemdebugsnapshot counters;
void declarative_xui_counters_begin(void){xrtMemDebugSnapshot(&counters);}
void declarative_xui_counters_end(void)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(counters.AllocCount==now.AllocCount && counters.FreeCount==now.FreeCount && counters.ReallocCount==now.ReallocCount);
}
