#include "app/xide_app.h"
#include "ui/workbench_windows.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture { const char* directory; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks;if(!(e)){fprintf(stderr,"dock_windows_view_test:%d: %s\n",__LINE__,#e);f->failed=1;goto cleanup;} } while(0)
static int settle(xide_app* a)
{
    if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0;
    xideWorkbenchLayout(a);
    return xuiLayout(a->context)==XUI_OK && !a->frameError;
}
static int click(xide_app* a,xui_rect_t rect)
{
    float x=rect.fX+rect.fW/2,y=rect.fY+rect.fH/2;
    return rect.fW>0 && rect.fH>0 && xuiInputPointerMove(a->context,x,y,0)==XUI_OK &&
        xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static int selectWindow(xide_app* a,int bar,xui_widget menu,int action)
{
    if(xuiMenuBarOpenItem(a->menuBar,bar)!=XUI_OK || !settle(a)) return 0;
    for(int i=0;i<xuiMenuGetItemCount(menu);++i) if(xuiMenuGetItem(menu,i)->iValue==action) {
        xui_rect_t rect=xuiMenuGetItemRect(menu,i),world=xuiWidgetGetWorldRect(menu);
        rect.fX+=world.fX;rect.fY+=world.fY;return click(a,rect);
    }
    return 0;
}
static int states(xide_app* a,xui_widget menu)
{
    int seen[XIDE_WINDOW_COUNT]={0};
    for(int i=0;i<xuiMenuGetItemCount(menu);++i) {
        const xui_menu_item_t* item=xuiMenuGetItem(menu,i);
        if(item->iValue<XIDE_ACTION_VIEW_COMMANDS || item->iValue>XIDE_ACTION_VIEW_USAGES) continue;
        int id=item->iValue-XIDE_ACTION_VIEW_COMMANDS,visible=xideWorkbenchWindowVisible(a,id);
        xide_action_state query=xideActionQuery(a,item->iValue);
        if(++seen[id]!=1 || item->iType!=XUI_MENU_ITEM_CHECK || visible<0 || !query.enabled ||
           query.checked!=visible || !!(item->iState&XUI_MENU_ITEM_CHECKED)!=visible ||
           !(item->iState&XUI_MENU_ITEM_ENABLED) || strcmp(item->sText,xideActionFind(item->iValue)->title)) return 0;
    }
    for(int i=0;i<XIDE_WINDOW_COUNT;++i) if(seen[i]!=1) return 0;
    return 1;
}
static int run(void* data)
{
    fixture* f=data;if(f->started)return XGE_OK;f->started=1;
    xide_app a={0};int began=0,bar=-1;xui_widget menu=NULL;xvalue* saved=NULL;
    char* settings=xidePathResolve(f->directory,"settings.json");
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;
    CHECK(settings && xideWorkbenchWindowVisible(&a,XIDE_COMMANDS)==-1);
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(settle(&a));
    for(int i=0;i<xuiMenuBarGetItemCount(a.menuBar);++i) {
        const xui_menubar_item_t* item=xuiMenuBarGetItem(a.menuBar,i);
        if(!strcmp(item->sText,"视图")) {CHECK(bar<0);bar=i;menu=item->pMenu;}
    }
    CHECK(bar>=0 && states(&a,menu));
    for(int id=0;id<XIDE_WINDOW_COUNT;++id) {
        int action=XIDE_ACTION_VIEW_COMMANDS+id;xui_widget client=a.panels[id];
        if(xideWorkbenchWindowVisible(&a,id)) CHECK(selectWindow(&a,bar,menu,action));
        CHECK(xideWorkbenchWindowVisible(&a,id)==0 && states(&a,menu));
        CHECK(selectWindow(&a,bar,menu,action) && xideWorkbenchWindowVisible(&a,id)==1 && states(&a,menu));
        /* Native title-bar close must leave the same panel available to View. */
        CHECK(xuiDockPanelFloatWindow(a.dock,a.windows[id],(xui_rect_t){80,100,480,320})==XUI_OK && settle(&a));
        CHECK(states(&a,menu));
        xui_widget close=xuiDockPanelGetWindowCloseButtonWidget(a.dock,a.windows[id]);
        CHECK(close && click(&a,xuiWidgetGetWorldRect(close)));
        if(xideWorkbenchWindowVisible(&a,id)!=0 || !states(&a,menu))
            fprintf(stderr,"native close: id=%d visible=%d checked=%d\n",id,xideWorkbenchWindowVisible(&a,id),xideActionQuery(&a,action).checked);
        CHECK(xideWorkbenchWindowVisible(&a,id)==0 && states(&a,menu));
        CHECK(selectWindow(&a,bar,menu,action) && xideWorkbenchWindowVisible(&a,id)==1 && states(&a,menu));
        CHECK(a.panels[id]==client && xuiDockPanelGetWindowClient(a.dock,a.windows[id])==client);
    }
    /* No surviving sibling or editor pane may be assumed by a reopen. */
    for(int id=0;id<XIDE_WINDOW_COUNT;++id) CHECK(xuiDockPanelHideWindow(a.dock,a.windows[id])==XUI_OK);
    CHECK(settle(&a) && states(&a,menu));
    for(int id=XIDE_WINDOW_COUNT-1;id>=0;--id)
        CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_COMMANDS+id) && xideWorkbenchWindowVisible(&a,id)==1 && states(&a,menu));
    xui_dock_pane_info_t editorPane={0};editorPane.iSize=sizeof(editorPane);
    CHECK(xuiDockPanelGetPaneInfo(a.dock,a.documentPane,&editorPane)==XUI_OK && editorPane.tRect.fH>a.viewportHeight/2);
    /* A C-only editor still gets a separate bottom tool pane. */
    for(int id=XIDE_HELP;id<=XIDE_USAGES;++id) CHECK(xuiDockPanelHideWindow(a.dock,a.windows[id])==XUI_OK);
    CHECK(xuiDockPanelHideWindow(a.dock,a.windows[XIDE_XLANG_DEMO])==XUI_OK && settle(&a));
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_HELP));
    xui_dock_window_info_t cEditor={0},help={0};cEditor.iSize=sizeof(cEditor);help.iSize=sizeof(help);
    CHECK(xuiDockPanelGetWindowInfo(a.dock,a.windows[XIDE_C_DEMO],&cEditor)==XUI_OK &&
        xuiDockPanelGetWindowInfo(a.dock,a.windows[XIDE_HELP],&help)==XUI_OK && cEditor.iPane!=help.iPane);
    for(int id=XIDE_HELP+1;id<=XIDE_USAGES;++id) CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_COMMANDS+id));
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_XLANG) && states(&a,menu));
    CHECK(xuiDockPanelAutoHideWindow(a.dock,a.windows[XIDE_COMMANDS])==XUI_OK && settle(&a) && states(&a,menu));
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_COMMANDS) && xideWorkbenchWindowVisible(&a,XIDE_COMMANDS)==0 && states(&a,menu));
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_PROPERTIES) && xideWorkbenchWindowVisible(&a,XIDE_PROPERTIES)==0);
    CHECK(xuiDockPanelSaveState(a.dock,&saved)==XUI_OK);
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_COMMANDS) && selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_PROPERTIES));
    CHECK(xuiDockPanelLoadState(a.dock,saved)==XUI_OK && settle(&a) && states(&a,menu));
    CHECK(xideWorkbenchWindowVisible(&a,XIDE_COMMANDS)==0 && xideWorkbenchWindowVisible(&a,XIDE_PROPERTIES)==0);
    CHECK(selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_COMMANDS) && selectWindow(&a,bar,menu,XIDE_ACTION_VIEW_PROPERTIES));
    CHECK(xuiMenuBarOpenItem(a.menuBar,bar)==XUI_OK && settle(&a));
    CHECK(xuiUpdate(a.context,0)==XUI_OK);
    xui_rect_i_t rect={0,0,a.viewportWidth,a.viewportHeight};
    CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0xffffffff)==XUI_OK && xuiRender(a.context,a.target,&rect,1)==XUI_OK);
    char* image=xidePathResolve(f->directory,"view-menu.png");CHECK(image);
    int captured=xideAppCapture(&a,image);free(image);CHECK(captured==XGE_OK);
    printf("dock_windows_view_test passed: %d checks; 12 checkable windows, actual View and native close clicks, floating/auto-hide, all-hidden recovery, unchanged clients and loaded layout state.\n",f->checks);
cleanup:
    xuiDockPanelStateFree(saved);if(began)xgeEnd();xideAppDestroy(&a);free(settings);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{
    fixture* f=data;if(!SetThreadDesktop(f->desktop))return 1;
    xge_desc_t desc={0};desc.iWidth=1400;desc.iHeight=1000;desc.sTitle="IDE View window verification";desc.iFlags=XGE_INIT_WINDOW;
    int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(run,f);xgeUnit();
    return result==XGE_OK && f->started && !f->failed ? 0:1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;
    fixture f={0};f.directory=argv[1];wchar_t name[100];swprintf(name,100,L"XlangDockView-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
    if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread) {if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}
    CloseDesktop(f.desktop);return (int)code;
}
