#include "app/xide_app.h"
#include "ui/home_view.h"
#include "ui/editor_view.h"
#include "ui/workbench_windows.h"
#include "ui/workspace_view.h"
#include "ui/layout_state.h"
#include "ui/template_view.h"
#include "services/session.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include "theme/theme.h"
#include "catalog_ready.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int checks,started,failed;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"home_view_test:%d: %s (%s)\n",__LINE__,#e,error.message);failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a) {
    for(int i=0;i<3;++i) {
        if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;
        xideFileDialogPoll(a);xideMessagePoll(a);xideEditorPoll(a);xideWorkbenchLayout(a);
        if(xuiLayout(a->context)!=XUI_OK || a->frameError)return 0;
    }
    return 1;
}
static int click(xide_app* a,xui_widget w) {
    xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return r.fW>0 && r.fH>0 && xuiInputPointerMove(a->context,x,y,0)==XUI_OK &&
        xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static int choose(xide_app* a,const char* path) {
    xui_file_dialog d=xideFileDialogCurrent(a);char* dir=xidePathDirectory(path);
    int okay=d && dir && xuiFileDialogSetDirectory(d,dir)==XUI_OK &&
        xuiInputSetText(xuiFileDialogGetNameInputWidget(d),xidePathFilename(path))==XUI_OK;
    free(dir);return okay && settle(a) && click(a,xuiFileDialogGetOkButtonWidget(d));
}
static int pickTemplate(xide_app* a,int index) {
    xui_widget list=xideHomeControl(a,XIDE_HOME_TEMPLATES);xui_rect_t r=xuiListViewGetItemRect(list,index),world=xuiWidgetGetWorldRect(list);
    float x=world.fX+r.fX+30,y=world.fY+r.fY+r.fH/2;
    return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a) && xuiListViewGetSelected(list)==index;
}
static int closeHomeTab(xide_app* a) {
    xui_dock_window_info_t info={.iSize=sizeof(info)};xui_dock_panel_metrics_t metrics={.iSize=sizeof(metrics)};
    if(xuiDockPanelGetWindowInfo(a->dock,a->windows[XIDE_HOME],&info)!=XUI_OK || !info.bClosable ||
       xuiDockPanelGetMetrics(a->dock,&metrics)!=XUI_OK)return 0;
    float x=0,y=0;xui_dock_hit_t hit={.iSize=sizeof(hit)};
    if(info.tTabRect.fW>0) {
        float button=fminf(metrics.fButtonSize,fmaxf(0,info.tTabRect.fH-6));
        x=info.tTabRect.fX+info.tTabRect.fW-5-button/2;y=info.tTabRect.fY+info.tTabRect.fH/2;
        if(xuiDockPanelHitTest(a->dock,x,y,&hit)!=XUI_OK || hit.iType!=XUI_DOCK_PANEL_HIT_PANE_TAB_CLOSE || hit.iWindow!=a->windows[XIDE_HOME])return 0;
    } else {
        /* XUI uses caption chrome for a pane with one document. Locate its
           real close hit rather than assuming an unrendered tab rectangle. */
        xui_dock_pane_info_t pane={.iSize=sizeof(pane)};int found=0;
        if(xuiDockPanelGetPaneInfo(a->dock,info.iPane,&pane)!=XUI_OK)return 0;
        y=pane.tCaptionRect.fY+pane.tCaptionRect.fH/2;
        for(int px=pane.tCaptionRect.fX+pane.tCaptionRect.fW-1;px>=pane.tCaptionRect.fX;--px) {
            if(xuiDockPanelHitTest(a->dock,(float)px,y,&hit)==XUI_OK && hit.iType==XUI_DOCK_PANEL_HIT_PANE_CLOSE) {x=(float)px;found=1;break;}
        }
        if(!found)return 0;
    }
    xui_rect_t dock=xuiWidgetGetWorldRect(a->dock);x+=dock.fX;y+=dock.fY;
    return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a) && xideWorkbenchWindowVisible(a,XIDE_HOME)==0;
}
static int shot(xide_app* a,const char* root,const char* name) {
    char* path=xidePathResolve(root,name);xui_rect_i_t rect={0,0,a->viewportWidth,a->viewportHeight};
    int okay=path && xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffffffff)==XUI_OK &&
        xuiRender(a->context,a->target,&rect,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;free(path);return okay;
}
static xvalue* legacyLayout(const xvalue* layout) {
    size_t size=0;char* json=xrtJsonStringify(layout,0,&size);if(!json)return NULL;
    char* out=malloc(size*2+1);if(!out) {xrtFree(json);return NULL;}
    size_t used=0;
    for(size_t i=0;i<size;) {
        if(i+6<=size && !memcmp(json+i,"\"home\"",6)) {memcpy(out+used,"\"xlang_demo\"",12);used+=12;i+=6;}
        else out[used++]=json[i++];
    }
    xvalue* result=xrtJsonParse((xstrview){out,used});free(out);xrtFree(json);return result;
}
static int run(void* raw) {
    xide_app a={0};xide_model_error error={0};int began=0;char* directory=NULL;char* settings=NULL;char* script=NULL;char* workspace=NULL;char* sessions=NULL;char* manifest=NULL;xide_project project={0};
    xvalue* layout=NULL;started=1;char* id=xideCreateId("home");directory=id?xidePathResolve(raw,id):NULL;free(id);
    CHECK(directory && xrtDirCreateAll(directory));settings=xidePathResolve(directory,"settings.json");sessions=xidePathResolve(directory,"sessions");script=xidePathResolve(directory,"中文 示例.xl");workspace=xidePathResolve(directory,"新工作空间.xlw");
    CHECK(settings && sessions && script && workspace && xrtFileWriteAtomic(script,(xbytesview){(const unsigned char*)"print(1)\n",9}));
    manifest=xidePathResolve(directory,"已有工程.xlp");CHECK(manifest && xideProjectCreate(&project,manifest,"已有工程",script,&error) && xideProjectSave(&project,NULL,&error));
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&a) && settle(&a));a.hidden=0;
    CHECK(!a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1 && a.documents.count==0);
    CHECK(!xideActionNamed("view.xlangDemo") && !xideActionNamed("view.cDemo") && !strcmp(xideHomeTutorialUrl(),"https://xcompiler.com/book/index.html"));
    xui_widget table=xideHomeControl(&a,XIDE_HOME_HISTORY);CHECK(table && xuiTableViewGetColumnCount(table)==3 && xuiTableViewGetRowCount(table)==0);
    CHECK(shot(&a,raw,"home-empty.png"));
    CHECK(xuiWidgetGetType(xideHomeControl(&a,XIDE_HOME_CLEAR))==xuiWidgetFindType(a.context,"hyperlink") && xuiWidgetGetType(xideHomeControl(&a,XIDE_HOME_TUTORIAL))==xuiButtonGetType(a.context));
    xui_rect_t openRect=xuiWidgetGetWorldRect(xideHomeControl(&a,XIDE_HOME_OPEN)),tutorial=xuiWidgetGetWorldRect(xideHomeControl(&a,XIDE_HOME_TUTORIAL));
    CHECK(tutorial.fX>openRect.fX+openRect.fW && tutorial.fY==openRect.fY && tutorial.fH==openRect.fH);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW)) && xideDialogIsOpen(xideHomeNewWindow(&a)) && !xideActionQuery(&a,XIDE_ACTION_HOME_OPEN).enabled);
    CHECK(xuiListViewGetItemCount(xideHomeControl(&a,XIDE_HOME_TEMPLATES))==2 && shot(&a,raw,"new-template-xlang.png"));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)) && !xideDialogIsOpen(xideHomeNewWindow(&a)) && !a.activeDocument);
    for(int language=0;language<2;++language) {
        CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW)) && pickTemplate(&a,language));
        if(language==1)CHECK(shot(&a,raw,"new-template-c.png"));
        CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CREATE)) && a.activeDocument && a.activeDocument->open && !a.activeDocument->path && !strcmp(a.activeDocument->language,language==0?"xlang":"c") && !*xuiCodeEditGetText(a.activeDocument->editor) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1 && !xideDialogIsOpen(xideHomeNewWindow(&a)));
        CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    }
    /* Keyboard activation creates only the selected independent document. */
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW)) && xuiListViewSetSelected(xideHomeControl(&a,XIDE_HOME_TEMPLATES),0)==XUI_OK && xuiSetFocusWidget(a.context,xideHomeControl(&a,XIDE_HOME_TEMPLATES))==XUI_OK);
    CHECK(xuiInputKeyDownEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && settle(&a) && a.activeDocument && !strcmp(a.activeDocument->language,"xlang"));
    CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a));
    xide_workspace prepared={0};int preparedOkay=xideWorkspaceCreate(&prepared,"首页打开测试",&error) && xideWorkspaceSave(&prepared,workspace,&error);xideWorkspaceClear(&prepared);CHECK(preparedOkay);
    CHECK(closeHomeTab(&a));
    a.sessionSwitchConfirmed=1;int switched=xideWorkspaceOpen(&a,workspace,0,&error);a.sessionSwitchConfirmed=0;
    CHECK(switched && settle(&a) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideActionExecute(&a,XIDE_ACTION_VIEW_HOME)==XIDE_ACTION_DISPATCHED && settle(&a));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && xideFileDialogCurrent(&a));
    CHECK(!strcmp(xuiFileDialogGetFilterPattern(xideFileDialogCurrent(&a),0),"*.xlw;*.xlp;*.xl;*.c") && choose(&a,workspace));
    CHECK(a.workspaceModel.manifest && xidePathEqual(a.workspaceModel.manifest,workspace) && xideSettingsRecentCount(&a.preferences)==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && choose(&a,script) && a.activeDocument && xidePathEqual(a.activeDocument->path,script));
    CHECK(xideSettingsRecentCount(&a.preferences)==2 && xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a) && xuiTableViewGetRowCount(table)==2);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && choose(&a,workspace) && !a.activeDocument);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && xideFileDialogCurrent(&a));
    CHECK(xuiFileDialogCancel(xideFileDialogCurrent(&a))==XUI_OK && settle(&a) && !xideFileDialogCurrent(&a));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && choose(&a,manifest) && a.projectSessionPath && xidePathEqual(a.projectSessionPath,manifest) && a.workspaceModel.projectCount==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN)) && choose(&a,workspace) && !a.projectSessionPath);
    CHECK(xuiTableViewGetSelectionMode(table)==XUI_TABLE_VIEW_SELECTION_ROW);
    xui_style_property_t property={0};CHECK(xuiWidgetGetResolvedStyleProperty(table,"tableview.border.width",&property)==XUI_OK && property.tValue.fFloat==0);
    CHECK(xuiWidgetGetResolvedStyleProperty(table,"tableview.focus.color",&property)==XUI_OK && property.tValue.iColor==0);
    /* A suspended recent-open command must survive a dirty-workspace prompt. */
    a.workspaceModel.dirty=1;xideHomeRemember(&a,workspace);
    CHECK(settle(&a) && xuiTableViewSetSelectedRow(table,0)==XUI_OK && xuiSetFocusWidget(a.context,table)==XUI_OK);
    CHECK(xuiInputKeyDownEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && settle(&a) && xideMessageCurrent(&a));
    CHECK(click(&a,xuiMsgBoxGetButtonWidget(xideMessageCurrent(&a),2)) && a.workspaceModel.dirty && !xideMessageBusy(&a));
    CHECK(xuiInputKeyDownEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && settle(&a) && xideMessageCurrent(&a));
    CHECK(click(&a,xuiMsgBoxGetButtonWidget(xideMessageCurrent(&a),1)) && !a.workspaceModel.dirty && !xideMessageBusy(&a) && xidePathEqual(a.workspaceModel.manifest,workspace));
    /* Enter and real double-click activation, including source replacement. */
    xideHomeRemember(&a,script);CHECK(settle(&a) && xuiTableViewSetSelectedRow(table,0)==XUI_OK && xuiSetFocusWidget(a.context,table)==XUI_OK);
    CHECK(xuiInputKeyDownEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && settle(&a) && a.activeDocument && xidePathEqual(a.activeDocument->path,script));
    CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a));
    xui_rect_t cell={0},world=xuiWidgetGetWorldRect(table);CHECK(xuiTableViewGetCellRect(table,0,0,&cell)==XUI_OK);
    float x=world.fX+cell.fX+20,y=world.fY+cell.fY+cell.fH/2;
    for(int i=0;i<2;++i)CHECK(xuiInputPointerDown(a.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&a));
    CHECK(a.activeDocument && xidePathEqual(a.activeDocument->path,script));
    CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a));
    for(int theme=0;theme<IDE_THEME_COUNT;++theme) {
        CHECK(xideWorkbenchApplyTheme(&a,theme)==XUI_OK && settle(&a));
        xui_widget outer=xuiWidgetGetParent(xuiWidgetGetParent(table));xui_style_property_t border={0};
        CHECK(xuiWidgetGetResolvedStyleProperty(outer,"panel.border.color",&border)==XUI_OK && border.tValue.iColor==ideThemeColor(theme,IDE_BORDER));
        char name[64];snprintf(name,sizeof(name),"home-theme-%d.png",theme);CHECK(shot(&a,raw,name));
    }
    /* Home layout follows pane width and does not create stacked demo editors. */
    for(int i=0;i<3;++i) {
        a.viewportWidth=i==0?900:i==1?1600:1280;a.viewportHeight=i==0?600:i==1?1000:800;
        CHECK(xuiInputViewport(a.context,(float)a.viewportWidth,(float)a.viewportHeight)==XUI_OK && settle(&a));
        xui_rect_t page=xuiWidgetGetWorldRect(a.panels[XIDE_HOME]),history=xuiWidgetGetWorldRect(table);
        CHECK(history.fW>0 && history.fH>0 && history.fX>=page.fX && history.fX+history.fW<=page.fX+page.fW+1 && history.fY+history.fH<=page.fY+page.fH+1);
    }
    CHECK(xuiTableViewSetSelectedRow(table,1)==XUI_OK && xuiSetFocusWidget(a.context,table)==XUI_OK && settle(&a) && shot(&a,raw,"home-history.png"));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW)) && xideDialogIsOpen(xideHomeNewWindow(&a)));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)) && xuiListViewSetSelected(xideHomeControl(&a,XIDE_HOME_TEMPLATES),-1)==XUI_OK);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW)) && !xuiWidgetGetEnabled(xideHomeControl(&a,XIDE_HOME_CREATE)));
    CHECK(pickTemplate(&a,1) && xuiWidgetGetEnabled(xideHomeControl(&a,XIDE_HOME_CREATE)) && strstr(xuiLabelGetText(xideHomeControl(&a,XIDE_HOME_TEMPLATE_DESCRIPTION)),"C 文件"));
    /* The template shell uses the shared dialog layout: a real minimum size,
       a fixed, separated action bar and a body that can scroll independently. */
    char* font=xidePathResolve(NULL,a.uiFontPath);CHECK(font);
    for(int i=0;i<3;++i) {
        int size=i==0?16:i==1?24:40;
        int changed=xideUiFontApply(&a,font,size,&error);CHECK(changed && settle(&a));
        xui_widget window=xideHomeNewWindow(&a),footer=xideDialogGetFooterWidget(window),scroll=xideDialogGetScrollViewWidget(window);
        xui_rect_t bottom=xuiWidgetGetWorldRect(footer),body=xuiWidgetGetWorldRect(scroll),button=xuiWidgetGetWorldRect(xideHomeControl(&a,XIDE_HOME_CREATE));
        CHECK(footer && xideDialogGetDividerWidget(window) && bottom.fY>=body.fY+body.fH-1 && button.fY>=bottom.fY && button.fY+button.fH<=bottom.fY+bottom.fH+1);
        CHECK(xuiListViewGetFont(xideHomeControl(&a,XIDE_HOME_TEMPLATES))==a.uiFont);
    }
    int restoredFont=xideUiFontApply(&a,font,16,&error);free(font);CHECK(restoredFont && settle(&a) && shot(&a,raw,"new-template-dark.png"));
    CHECK(xuiInputKeyDownEx(a.context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && settle(&a) && !xideDialogIsOpen(xideHomeNewWindow(&a)) && !a.activeDocument);
    layout=xideLayoutCapture(&a,&error);CHECK(layout);xvalue* old=legacyLayout(layout);CHECK(old);
    int restored=xideLayoutRestore(&a,old,&error);xrtValueRelease(old);
    CHECK(restored && xideWorkbenchShowHome(&a)==XUI_OK && settle(&a) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CLEAR)) && xideSettingsRecentCount(&a.preferences)==0 && xuiTableViewGetRowCount(table)==0);
    CHECK(xideSettingsLoad(&a.preferences,settings,&error) && xideSettingsRecentCount(&a.preferences)==0);
    a.noSession=0;CHECK(xideSessionStart(&a) && settle(&a) && xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideTestCatalogReady(&a) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    /* Home is an ordinary closable tab, sharing a pane with source files. */
    CHECK(xideEditorOpen(&a,script,&error) && settle(&a) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    xide_document* coexist=a.activeDocument;xui_dock_window_info_t home={.iSize=sizeof(home)},file={.iSize=sizeof(file)};
    CHECK(xuiDockPanelGetWindowInfo(a.dock,a.windows[XIDE_HOME],&home)==XUI_OK && home.bClosable && xuiDockPanelGetWindowInfo(a.dock,coexist->window,&file)==XUI_OK && home.iPane==file.iPane);
    CHECK(shot(&a,raw,"home-with-file.png"));
    CHECK(closeHomeTab(&a) && a.activeDocument==coexist && coexist->open);
    xide_document* other=xideDocumentNew(&a.documents,"c",&error);CHECK(other && xideEditorShow(&a,other)==XUI_OK && settle(&a) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideEditorClose(&a,other,1,&error) && xideEditorClose(&a,coexist,1,&error) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideActionExecute(&a,XIDE_ACTION_VIEW_HOME)==XIDE_ACTION_DISPATCHED && settle(&a) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(xideEditorOpen(&a,script,&error) && settle(&a));coexist=a.activeDocument;
    CHECK(xideWorkbenchShowHome(&a)==XUI_OK && settle(&a) && !a.activeDocument && coexist->open);
    CHECK(shot(&a,raw,"home-with-file-selected.png"));
    CHECK(xideEditorClose(&a,coexist,1,&error) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(xideEditorOpen(&a,script,&error) && xideWorkbenchShowHome(&a)==XUI_OK && settle(&a));coexist=xideDocumentsFind(&a.documents,script);
    CHECK(coexist && closeHomeTab(&a) && a.activeDocument==coexist && xuiGetFocusWidget(a.context)==coexist->editor);
    CHECK(xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideTestCatalogReady(&a) && settle(&a) && a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0 && xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideTestCatalogReady(&a) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideWorkbenchShowHome(&a)==XUI_OK && xideEditorOpen(&a,script,&error) && settle(&a) && xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideTestCatalogReady(&a) && settle(&a) && a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(xideWorkbenchShowHome(&a)==XUI_OK && settle(&a) && !a.activeDocument && xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideTestCatalogReady(&a) && settle(&a) && !a.activeDocument && xideDocumentsFind(&a.documents,script) && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(xuiDockPanelGetPaneActiveWindow(a.dock,a.documentPane)==a.windows[XIDE_HOME]);
    puts("home_view_test: compact Home, 2 source templates, modal cancel/create/Enter, combined open filter and file/project/workspace routes, flat row-selection styling, Enter/double-click history, 4 themes, 3 pane sizes and persistent clear");
cleanup:
    xrtValueRelease(layout);if(began)xgeEnd();xideAppDestroy(&a);xideProjectClear(&project);free(directory);free(settings);free(script);free(workspace);free(sessions);free(manifest);xgeQuit();
    if(!failed)printf("home_view_test passed: %d checks\n",checks);
    return XGE_OK;
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="IDE Home verification";d.iFlags=XGE_INIT_WINDOW;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK && started && !failed?0:1;
}
