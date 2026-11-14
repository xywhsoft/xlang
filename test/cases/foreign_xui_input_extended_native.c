/* Independent C caller of the same actual SDK and CPU-only backend. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_input_extended.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(XGE_TEST_STATIC)
#include <xge_static_xrt_contract.h>
#else
#include <xge_host_xrt_contract.h>
static xge_host_xrt_proc resolve(void* user,const char* name)
{
 (void)user;
#define XGE_HOST_XRT_SLOT(symbol) if(!strcmp(name,#symbol))return (xge_host_xrt_proc)&symbol;
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
 return NULL;
}
#endif
static int commands;
static int command(xui_widget widget,const xui_event_t* event,void* user)
{
 (void)widget;(void)user;assert(event->iType==XUI_EVENT_COMMAND && event->iCommand==71);
 assert(!strcmp(event->sText,commands ? "queued":"hot"));++commands;return XUI_EVENT_DISPATCH_STOP;
}
static void ime(xui_context ui,const char* text,int active,int replacement,int start,int end)
{
 xui_ime_composition_t value={sizeof(value),text,(int)strlen(text),active,(int)strlen(text),0,(int)strlen(text),replacement,start,end,7};
 assert(xuiInputImeCompositionEx(ui,&value)==XUI_OK);assert(xuiDispatchPendingEvents(ui)==XUI_OK);
}
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=NULL;
 assert(fixtureInputProxy(&proxy)==XUI_OK && fixtureLayoutFont(proxy,&font)==XUI_OK);
 const xui_context_desc_t options={sizeof(options),XUI_SYSTEM_FONT_DISABLED,0,0};
 assert(xuiCreateOwnedEx(proxy,&options,&ui)==XUI_OK);assert(xuiSetDefaultFont(ui,font)==XUI_OK);
 xui_widget root=NULL,input=NULL,editor=NULL,target=NULL;
 assert(xuiWidgetCreate(ui,&root)==XUI_OK && xuiInputCreate(ui,&input,NULL)==XUI_OK && xuiCodeEditCreate(ui,&editor,NULL)==XUI_OK && xuiWidgetCreate(ui,&target)==XUI_OK);
 assert(root&&input&&editor&&target);assert(xuiSetRootWidget(ui,root)==XUI_OK);
 assert(xuiWidgetAddChild(root,input)==XUI_OK&&xuiWidgetAddChild(root,editor)==XUI_OK&&xuiWidgetAddChild(root,target)==XUI_OK);
 assert(xuiWidgetSetFocusable(target,1)==XUI_OK&&xuiWidgetSetImeMode(target,XUI_IME_ENABLED)==XUI_OK);
 assert(xuiSetFocusWidget(ui,input)==XUI_OK&&xuiInputSetText(input,"abc")==XUI_OK&&xuiInputSetSelection(input,1,2)==XUI_OK);
 ime(ui,"\xe4\xbd\xa0",1,0,0,0);assert(!strcmp(xuiInputGetText(input),"abc"));
 ime(ui,"\xe5\xa5\xbd",0,1,1,2);assert(!strcmp(xuiInputGetText(input),"a\xe5\xa5\xbd""c"));
 assert(xuiSetFocusWidget(ui,editor)==XUI_OK&&xuiCodeEditSetTextLength(editor,"abc",3)==XUI_OK);
 ime(ui,"\xe4\xbd\xa0",0,1,1,2);xstrview snapshot={0};
 assert(xuiCodeEditGetTextView(editor,&snapshot)==XUI_OK&&snapshot.Size==5&&!memcmp(snapshot.Data,"a\xe4\xbd\xa0""c",5));
 assert(xuiSetFocusWidget(ui,target)==XUI_OK);uint32_t flags=37;
 assert(xuiInputKeyDownEx(ui,-1,0,&flags)==XUI_ERROR_INVALID_ARGUMENT&&flags==0);
 assert(xuiInputTextEx(ui,0xd800,&flags)==XUI_ERROR_INVALID_ARGUMENT&&flags==0);
 assert(xuiInputKeyUpEx(ui,'A',UINT32_C(0x80000001),&flags)==XUI_OK&&flags==0&&xuiInputGetModifiers(ui)==XUI_MOD_SHIFT);
 assert(xuiWidgetSetEventHandler(target,XUI_EVENT_COMMAND,command,NULL)==XUI_OK);
 assert(xuiHotKeyRegisterCommand(ui,target,XUI_KEY_F2,XUI_MOD_CTRL,71,"hot",NULL)==XUI_OK);
 assert(xuiInputKeyDownEx(ui,XUI_KEY_F2,XUI_MOD_CTRL,&flags)==XUI_OK&&flags==5&&commands==1);
 assert(xuiCommandDispatch(ui,target,71,"queued",NULL)==XUI_OK&&commands==1);
 assert(xuiDispatchPendingEvents(ui)==XUI_OK&&commands==2);
 assert(xuiHotKeyUnregister(ui,target,XUI_KEY_F2,XUI_MOD_CTRL)==XUI_OK);
 assert(xuiWidgetSetEventHandler(target,XUI_EVENT_COMMAND,NULL,NULL)==XUI_OK);
 fixtureInputNativeHot(ui);fixtureInputBudget(0);fixtureInputNativeHot(ui);fixtureInputBudget(-1);
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK);
 fixtureInputCleanupCheck();
 xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);assert(!end.LiveCount&&!end.InvalidFreeCount&&!end.DoubleFreeCount);
 puts("InputEx native: actual SDK flags, commands, UTF-8 IME and cleanup passed");return 0;
}
