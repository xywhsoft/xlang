#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/font_view.h"
#include "ui/import_view.h"
#include "ui/recovery_view.h"
#include "ui/settings_view.h"
#include "ui/template_view.h"
#include "ui/configuration_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/rename_view.h"
#include "ui/toolchain_view.h"
#include "ui/file_changes_view.h"
#include "ui/goto_view.h"
#include "ui/workspace_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
typedef struct fixture {const char* directory;HDESK desktop;int started,failed,checks;} fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"dialog_layout_test:%d: %s; window=%s; font=%d; %s\n",__LINE__,#e,active ? xuiWindowGetTitle(active):"startup",a.uiFontSize,error.message);f->failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a) {
 for(int i=0;i<3;++i){xideWorkbenchLayout(a);if(xuiDispatchPendingEvents(a->context)!=XUI_OK || xuiLayout(a->context)!=XUI_OK || a->frameError)return 0;}
 return 1;
}
static int capture(xide_app* a,const char* path) {
 xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};
 return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;
}
static int within(xui_rect_t inner,xui_rect_t outer) {
 return inner.fX>=outer.fX-1 && inner.fY>=outer.fY-1 && inner.fX+inner.fW<=outer.fX+outer.fW+1 && inner.fY+inner.fH<=outer.fY+outer.fH+1;
}
static int footerFits(xui_context c,xui_widget node,xui_rect_t area) {
 if(!xuiWidgetGetVisible(node))return 1;
 if(xuiWidgetGetType(node)==xuiButtonGetType(c) && !within(xuiWidgetGetWorldRect(node),area))return 0;
 for(xui_widget child=xuiWidgetGetFirstChild(node);child;child=xuiWidgetGetNextSibling(child))if(!footerFits(c,child,area))return 0;
 return 1;
}
static int verify(void* user) {
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;
 xide_app a={0};xide_model_error error={0};xui_widget active=NULL;int began=0;
 char* settings=xidePathResolve(f->directory,"settings.json");char* entry=xidePathResolve(f->directory,"main.xl");char* manifest=xidePathResolve(f->directory,"dialog.xlp");xide_project project={0};
 a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;
 CHECK(settings && entry && manifest && xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)"func main()\nend\n",16}));
 CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideWorkspaceCreate(&a.workspaceModel,"Dialog layout",&error));
 CHECK(xideProjectCreate(&project,manifest,"Dialog tests",entry,&error) && xideProjectSave(&project,NULL,&error) && xideWorkspaceAddProject(&a.workspaceModel,manifest,"dialogs",&error) && xideWorkspaceViewRefresh(&a)==XUI_OK && xideWorkspaceViewSelectProject(&a,"dialogs"));
 CHECK(xideResourceViewOpenNew(&a));active=xideResourceViewWindow(&a);CHECK(active && settle(&a));CHECK(xideDialogSetOpen(active,0)==XUI_OK);
 char longPath[4096];memset(longPath,'W',sizeof(longPath)-1);longPath[sizeof(longPath)-1]=0;
 CHECK(xuiInputSetText(xideFontViewControl(&a,XIDE_FONT_PATH),longPath)==XUI_OK);
 const char* items[]={longPath,"short"};xui_widget candidates=xideImportViewControl(&a,XIDE_IMPORT_CANDIDATES);
 CHECK(xuiComboBoxSetItems(candidates,items,2)==XUI_OK);
 /* The current template form is created lazily after template selection. */
 xide_template_registry templates={0};CHECK(xideTemplateRegistryLoad(&templates,NULL,&error));CHECK(xideTemplateViewOpenDefinition(&a,&templates.items[1]));xideTemplateRegistryClear(&templates);CHECK(xideDialogSetOpen(xideTemplateViewWindow(&a),0)==XUI_OK);
 xui_widget windows[]={xideFontViewWindow(&a),xideImportViewWindow(&a),xideRecoveryViewWindow(&a),xideSettingsViewWindow(&a),xideTemplateViewWindow(&a),xideConfigurationViewWindow(&a),xideReferencesViewWindow(&a),xideRenameViewWindow(&a),xideToolchainViewWindow(&a),xideFileChangesViewWindow(&a),xideGotoViewWindow(&a),xideResourceViewWindow(&a)};
 const int fonts[]={16,20,24,40};
 for(size_t font=0;font<sizeof(fonts)/sizeof(*fonts);++font) {
  CHECK(xideUiFontApply(&a,a.uiFontPath,fonts[font],&error));
  for(size_t i=0;i<sizeof(windows)/sizeof(*windows);++i) {
   active=windows[i];CHECK(active && xideDialogSetOpen(active,1)==XUI_OK && settle(&a));
   xui_widget footer=xideDialogGetFooterWidget(active),line=xideDialogGetDividerWidget(active),scroll=xideDialogGetScrollViewWidget(active),content=xideDialogGetContentWidget(active);
   CHECK(footer && line && xuiWidgetGetParent(footer)==xuiWindowGetClientWidget(active));
   const int sizes[][2]={{500,340},{800,600},{1450,900}};
   for(size_t size=0;size<sizeof(sizes)/sizeof(*sizes);++size) {
    xui_rect_t r=xuiWidgetGetRect(active);r.fW=sizes[size][0];r.fH=sizes[size][1];
    CHECK(xuiWidgetSetRect(active,r)==XUI_OK && settle(&a));
    float mw=0,mh=0;CHECK(xuiWindowGetMinSize(active,&mw,&mh)==XUI_OK);r=xuiWidgetGetRect(active);
    CHECK(r.fW>=mw-1 && r.fH>=mh-1 && r.fW<=a.viewportWidth && r.fH<=a.viewportHeight);
    xui_rect_t bar=xuiWidgetGetWorldRect(footer),body=xuiWidgetGetWorldRect(scroll),client=xuiWidgetGetWorldRect(xuiWindowGetClientWidget(active));
    CHECK(within(bar,client) && body.fH>0 && body.fY+body.fH<=bar.fY+1 && footerFits(a.context,footer,bar));
    CHECK(xuiWidgetGetWorldRect(line).fW>=client.fW-1);
    float dx=0,dy=0;CHECK(xuiScrollModelGetMaxOffset(xuiScrollViewGetModel(scroll),&dx,&dy)==XUI_OK && dx==0);
    for(xui_widget row=xuiWidgetGetFirstChild(content);row;row=xuiWidgetGetNextSibling(row))if(xuiWidgetGetVisible(row)) {
     xui_rect_t cr=xuiWidgetGetRect(row),extent=xuiWidgetGetRect(content);CHECK(cr.fX>=0 && cr.fX+cr.fW<=extent.fW+1);
    }
    CHECK(xideDialogSetScroll(active,0,dy)==XUI_OK && settle(&a));
    xui_rect_t scrolled=xuiWidgetGetWorldRect(footer);CHECK(scrolled.fY==bar.fY && scrolled.fH==bar.fH && footerFits(a.context,footer,bar));
    CHECK(xideDialogSetScroll(active,0,0)==XUI_OK);
   }
   if(i==0) {
    xui_rect_t input=xuiWidgetGetWorldRect(xideFontViewControl(&a,XIDE_FONT_PATH));
    xui_rect_t row=xuiWidgetGetWorldRect(xuiWidgetGetParent(xideFontViewControl(&a,XIDE_FONT_PATH)));
    CHECK(input.fW>0 && input.fX+input.fW<=row.fX+row.fW+1);
   }
   if(i==1) {
    CHECK(xuiComboBoxOpen(candidates)==XUI_OK && settle(&a));
    xui_widget popup=xuiMenuGetPopupWidget(xuiComboBoxGetMenuWidget(candidates));
    CHECK(within(xuiWidgetGetWorldRect(xuiPopupGetPanelWidget(popup)),(xui_rect_t){0,0,a.viewportWidth,a.viewportHeight}));
    CHECK(xuiComboBoxClose(candidates)==XUI_OK && settle(&a));
   }
   if(font==0 && i<3){char name[40];snprintf(name,sizeof(name),"dialog-%u.png",(unsigned)i);char* path=xidePathResolve(f->directory,name);CHECK(path && capture(&a,path));free(path);}
   CHECK(xideDialogSetOpen(active,0)==XUI_OK);
  }
 }
 /* Even a host smaller than the preferred logical minima must retain the
  * actions. This is the 40-DIP/small-window case from the resizing regressions. */
 a.viewportWidth=700;a.viewportHeight=460;CHECK(xuiInputViewport(a.context,700,460)==XUI_OK);
 for(size_t i=0;i<sizeof(windows)/sizeof(*windows);++i) {
  active=windows[i];CHECK(xideDialogSetOpen(active,1)==XUI_OK && settle(&a));
  xui_widget footer=xideDialogGetFooterWidget(active);xui_rect_t client=xuiWidgetGetWorldRect(xuiWindowGetClientWidget(active));
  CHECK(within(xuiWidgetGetWorldRect(footer),client) && footerFits(a.context,footer,client));
  CHECK(xuiWidgetGetRect(xideDialogGetScrollViewWidget(active)).fH>0);
  CHECK(xideDialogSetOpen(active,0)==XUI_OK);
 }
 a.viewportWidth=1600;a.viewportHeight=1000;CHECK(xuiInputViewport(a.context,1600,1000)==XUI_OK && settle(&a));
 char message[12000];message[0]=0;for(int i=0;i<120;++i)strcat(message,"Long diagnostic message / selectable scrolling content.\n");
 a.hidden=0;xideMessageShow(&a,"Long message",message,XUI_MSGBOX_ICON_INFO);CHECK(settle(&a));
 xui_msgbox box=xideMessageCurrent(&a);CHECK(box && xuiWindowGetResizeEdges(xuiMsgBoxGetWindowWidget(box))==0);
 xui_widget details=NULL;for(xui_widget c=xuiWidgetGetFirstChild(xuiMsgBoxGetContentWidget(box));c;c=xuiWidgetGetNextSibling(c))if(xuiWidgetGetType(c)==xuiTextEditGetType(a.context))details=c;
 CHECK(details && !strcmp(xuiTextEditGetText(details),message));
 float maxX=0,maxY=0;CHECK(xuiScrollModelGetMaxOffset(xuiTextEditGetScrollModel(details),&maxX,&maxY)==XUI_OK && maxY>0);
 xui_widget button=xuiMsgBoxGetButtonWidget(box,0);xui_rect_t position=xuiWidgetGetWorldRect(button);
 CHECK(xuiTextEditSetScroll(details,0,maxY)==XUI_OK && settle(&a) && xuiWidgetGetWorldRect(button).fY==position.fY);
 CHECK(xuiInputPointerMove(a.context,position.fX+position.fW/2,position.fY+position.fH/2,0)==XUI_OK && xuiInputPointerDown(a.context,position.fX+position.fW/2,position.fY+position.fH/2,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a.context,position.fX+position.fW/2,position.fY+position.fH/2,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&a));
 xideMessagePoll(&a);CHECK(!xideMessageBusy(&a));a.hidden=1;
 printf("dialog_layout_test passed: %d checks; 12 dialogs, 4 font sizes, 3 resize targets, native minimum sizes, no horizontal overflow, fixed separated footer and content-only scrolling, 3 rendered screenshots.\n",f->checks);
cleanup:
 if(began)xgeEnd();
 xideAppDestroy(&a);xideProjectClear(&project);free(settings);free(entry);free(manifest);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user) {
 fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Dialog responsive layout regression";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
 int r=xgeInit(&d);if(r==XGE_OK)r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0:1;
}
int main(int argc,char** argv) {
 if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;
 fixture f={.directory=argv[1]};wchar_t name[80];swprintf(name,80,L"XlangDialogLayout-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return code;
}
