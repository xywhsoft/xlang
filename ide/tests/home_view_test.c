#include "app/xide_app.h"
#include "ui/home_view.h"
#include "ui/editor_view.h"
#include "ui/workbench_windows.h"
#include "ui/layout_state.h"
#include "ui/template_view.h"
#include "services/session.h"
#include "platform/path.h"
#include "theme/theme.h"
#include "catalog_ready.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW_FILE)) && a.activeDocument && a.activeDocument->open && xideWorkbenchWindowVisible(&a,XIDE_HOME)==0);
    CHECK(xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a) && !a.activeDocument && xideWorkbenchWindowVisible(&a,XIDE_HOME)==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW_PROJECT)) && xideTemplateViewIsOpen(&a));
    CHECK(click(&a,xideTemplateViewControl(&a,XIDE_TEMPLATE_CANCEL)) && !xideTemplateViewIsOpen(&a));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW_WORKSPACE)) && xideFileDialogCurrent(&a) && choose(&a,workspace));
    CHECK(a.workspaceModel.manifest && xidePathEqual(a.workspaceModel.manifest,workspace) && xideSettingsRecentCount(&a.preferences)==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN_FILE)) && choose(&a,script) && a.activeDocument && xidePathEqual(a.activeDocument->path,script));
    CHECK(xideSettingsRecentCount(&a.preferences)==2 && xideEditorClose(&a,a.activeDocument,1,&error) && settle(&a) && xuiTableViewGetRowCount(table)==2);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN_WORKSPACE)) && choose(&a,workspace) && !a.activeDocument);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN_PROJECT)) && xideFileDialogCurrent(&a));
    CHECK(xuiFileDialogCancel(xideFileDialogCurrent(&a))==XUI_OK && settle(&a) && !xideFileDialogCurrent(&a));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN_PROJECT)) && choose(&a,manifest) && a.projectSessionPath && xidePathEqual(a.projectSessionPath,manifest) && a.workspaceModel.projectCount==1);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_OPEN_WORKSPACE)) && choose(&a,workspace) && !a.projectSessionPath);
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
        char name[64];snprintf(name,sizeof(name),"home-theme-%d.png",theme);CHECK(shot(&a,raw,name));
    }
    /* Home layout follows pane width and does not create stacked demo editors. */
    for(int i=0;i<3;++i) {
        a.viewportWidth=i==0?900:i==1?1600:1280;a.viewportHeight=i==0?600:i==1?1000:800;
        CHECK(xuiInputViewport(a.context,(float)a.viewportWidth,(float)a.viewportHeight)==XUI_OK && settle(&a));
        xui_rect_t page=xuiWidgetGetWorldRect(a.panels[XIDE_HOME]),history=xuiWidgetGetWorldRect(table);
        CHECK(history.fW>0 && history.fH>0 && history.fX>=page.fX && history.fX+history.fW<=page.fX+page.fW+1 && history.fY+history.fH<=page.fY+page.fH+1);
    }
    CHECK(shot(&a,raw,"home-history.png"));
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
    puts("home_view_test: native Home actions, workspace create/open, file open, project dialog, Enter/double-click history, 4 themes, 3 pane sizes and persistent clear");
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
