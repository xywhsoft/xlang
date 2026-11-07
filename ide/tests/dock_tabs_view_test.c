#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct dock_fixture { const char* directory; int failed,started,checks; } dock_fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"dock_tabs_view_test:%d: %s; XRT=%s\n",__LINE__,#e,xrtErrorMessage(xrtGetError()));f->failed=1;goto cleanup;}}while(0)
static int frame(xide_app* a) { return xideAppFrame(a)==XGE_OK && !a->frameError && !xrtGetError(); }
static int pane(xide_app* a,xui_dock_pane_info_t* info)
{ *info=(xui_dock_pane_info_t){0};info->iSize=sizeof(*info);return xuiDockPanelGetPaneInfo(a->dock,a->documentPane,info)==XUI_OK; }
static int arrows(xide_app* a,xui_rect_t* back,xui_rect_t* next)
{ return xuiDockPanelGetPaneTabScrollRects(a->dock,a->documentPane,back,next)==XUI_OK; }
static int click(xide_app* a,xui_widget owner,xui_rect_t rect)
{
    xui_rect_t world=xuiWidgetGetWorldRect(owner);
    int types[]={XGE_EVENT_MOUSE_MOVE,XGE_EVENT_MOUSE_DOWN,XGE_EVENT_MOUSE_UP};
    if(rect.fW<=0 || rect.fH<=0)return 0;
    for(int i=0;i<3;++i) {
        xge_input_event_t event={0};event.iSize=sizeof(event);event.iType=types[i];event.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC;
        event.fX=(world.fX+rect.fX+rect.fW/2)*a->dpiScale;event.fY=(world.fY+rect.fY+rect.fH/2)*a->dpiScale;
        event.iButton=XUI_POINTER_BUTTON_LEFT;event.iButtons=i==1 ? XUI_POINTER_BUTTON_LEFT:0;
        if(xgeInputEventPost(&event)!=XGE_OK)return 0;
    }
    return frame(a);
}
static int capture(xide_app* a,const char* directory,const char* name)
{ char* path=xidePathResolve(directory,name);int okay=path && xideAppCapture(a,path)==XGE_OK;free(path);return okay; }
static int run(void* user)
{
    dock_fixture* f=user;if(f->started)return XGE_OK;f->started=1;
    xide_app a={0};xide_model_error error={0};xide_document* docs[14]={0};xvalue* state=NULL;
    xui_dock_pane_info_t info={0};xui_rect_t back={0},next={0};
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;
    CHECK(frame(&a) && xuiDockPanelGetTabScrollButtonsVisible(a.dock));
    docs[0]=xideDocumentNew(&a.documents,"xlang",&error);
    CHECK(docs[0] && xideEditorShow(&a,docs[0])==XUI_OK && frame(&a));
    CHECK(pane(&a,&info) && !info.bOverflow && arrows(&a,&back,&next) && back.fW<=0 && next.fW<=0);
    for(int i=1;i<14;++i) {
        docs[i]=xideDocumentNew(&a.documents,i%2 ? "c":"xlang",&error);
        CHECK(docs[i] && xideEditorShow(&a,docs[i])==XUI_OK);
    }
    CHECK(xideEditorShow(&a,docs[0])==XUI_OK && frame(&a) && pane(&a,&info));
    CHECK(info.bOverflow && info.iFirstVisibleTab==0 && info.iVisibleTabCount<info.iWindowCount);
    CHECK(arrows(&a,&back,&next) && back.fW<=0 && next.fW>0 && capture(&a,f->directory,"tabs-start.png"));
    int active=xuiDockPanelGetPaneActiveWindow(a.dock,a.documentPane),first=info.iFirstVisibleTab;
    CHECK(click(&a,a.dock,next) && pane(&a,&info) && info.iFirstVisibleTab>first);
    CHECK(xuiDockPanelGetPaneActiveWindow(a.dock,a.documentPane)==active && arrows(&a,&back,&next) && back.fW>0);
    CHECK(capture(&a,f->directory,"tabs-middle.png"));
    for(int guard=0;next.fW>0 && guard<20;++guard)CHECK(click(&a,a.dock,next) && arrows(&a,&back,&next));
    CHECK(next.fW<=0 && back.fW>0 && pane(&a,&info) && info.iFirstVisibleTab>1);
    CHECK(xuiDockPanelGetPaneActiveWindow(a.dock,a.documentPane)==active && capture(&a,f->directory,"tabs-end.png"));
    CHECK(click(&a,a.dock,back) && pane(&a,&info) && arrows(&a,&back,&next) && next.fW>0);
    CHECK(xuiDockPanelSaveState(a.dock,&state)==XUI_OK && xuiDockPanelLoadState(a.dock,state)==XUI_OK && frame(&a));
    CHECK(xuiDockPanelGetTabScrollButtonsVisible(a.dock) && arrows(&a,&back,&next) && (back.fW>0 || next.fW>0));
    CHECK(pane(&a,&info) && click(&a,a.dock,info.tOptionRect));
    xui_widget menu=xuiDockPanelGetOptionMenu(a.dock);int item=-1;
    CHECK(menu && xuiMenuIsOpen(menu));
    for(int i=0;i<xuiMenuGetItemCount(menu);++i) {
        const xui_menu_item_t* row=xuiMenuGetItem(menu,i);
        if(row && row->iValue==XUI_DOCK_PANEL_MENU_WINDOW_BASE+docs[1]->window)item=i;
    }
    CHECK(item>=0 && capture(&a,f->directory,"tabs-list.png"));
    CHECK(click(&a,menu,xuiMenuGetItemRect(menu,item)) && xuiDockPanelGetPaneActiveWindow(a.dock,a.documentPane)==docs[1]->window);
    xui_dock_window_info_t selected={0};selected.iSize=sizeof(selected);
    CHECK(xuiDockPanelGetWindowInfo(a.dock,docs[1]->window,&selected)==XUI_OK && selected.tTabRect.fW>0);
    CHECK(xuiDockPanelSetTabScrollButtonsVisible(a.dock,0)==XUI_OK && frame(&a) && arrows(&a,&back,&next) && back.fW<=0 && next.fW<=0);
    CHECK(xuiDockPanelSetTabScrollButtonsVisible(a.dock,1)==XUI_OK && frame(&a) && arrows(&a,&back,&next) && (back.fW>0 || next.fW>0));
    printf("dock_tabs_view_test passed: %d checks; native overflow arrows, forward/backward limits without selection changes, pane tab list, state reload and configurable visibility.\n",f->checks);
cleanup:
    xuiDockPanelStateFree(state);xideAppDestroy(&a);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    dock_fixture f={0};f.directory=argv[1];
    xge_desc_t desc={0};desc.iWidth=1100;desc.iHeight=850;desc.sTitle="IDE dock tab navigation";desc.iFlags=XGE_INIT_WINDOW;
    int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(run,&f);xgeUnit();
    return result==XGE_OK && f.started && !f.failed ? 0:1;
}
