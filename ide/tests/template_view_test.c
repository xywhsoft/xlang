#include "app/xide_app.h"
#include "ui/home_view.h"
#include "ui/template_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/dialog.h"
#include "ui/message_box.h"
#include "ui/main_toolbar.h"
#include "ui/workbench_windows.h"
#include "ui/file_dialog.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "platform/dpi_proxy.h"
#include "services/ui_font.h"
#include "catalog_ready.h"
#include "dialog_test_helpers.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int failed,started,checks;
static int websiteOpens;static wchar_t openedWebsite[2049];
static HINSTANCE WINAPI openWebsite(HWND owner,LPCWSTR verb,LPCWSTR path,LPCWSTR args,LPCWSTR directory,INT show){(void)owner;(void)verb;(void)args;(void)directory;(void)show;++websiteOpens;wcsncpy(openedWebsite,path,2048);openedWebsite[2048]=0;return (HINSTANCE)(INT_PTR)33;}
/* Exercise real event routing without opening a browser during acceptance. */
HINSTANCE (WINAPI* __imp_ShellExecuteW)(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,INT)=openWebsite;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"template_view_test:%d %s; %s; %s\n",__LINE__,#e,error.message,xideTemplateViewMessage(&a));failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a){for(int i=0;i<3;++i){if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideMessagePoll(a);xideFileDialogPoll(a);xideEditorPoll(a);xideWorkbenchLayout(a);if(xuiLayout(a->context)!=XUI_OK||a->frameError)return 0;}return 1;}
static int click(xide_app* a,xui_widget w){xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;return r.fW>0&&r.fH>0&&xuiInputPointerMove(a->context,x,y,0)==XUI_OK&&xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&settle(a);}
static int category(xide_app* a,int id){xui_widget tree=xideHomeControl(a,XIDE_HOME_CATEGORY);int n=xuiTreeViewGetVisibleIndexOfId(tree,id);xui_rect_t r=xuiTreeViewGetItemRect(tree,n),w=xuiWidgetGetWorldRect(tree);float x=w.fX+r.fX+r.fW/2,y=w.fY+r.fY+r.fH/2;return xuiInputPointerMove(a->context,x,y,0)==XUI_OK&&xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&settle(a);}
static int button(xide_app* a,int id){xui_widget w=xideTemplateViewControl(a,id);if(xideTestReveal(xideDialogGetScrollViewWidget(xideTemplateViewWindow(a)),w)!=XUI_OK||!settle(a))return 0;return click(a,w);}
static int shot(xide_app* a,const char* root,const char* name){char* path=xidePathResolve(root,name);xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};int okay=path&&xuiUpdate(a->context,0)==XUI_OK&&a->proxy.surfaceClear(&a->proxy,a->target,0xffffffff)==XUI_OK&&xuiRender(a->context,a->target,&r,1)==XUI_OK&&xideAppCapture(a,path)==XGE_OK;free(path);return okay;}
static int finish(xide_app* a){ULONGLONG until=GetTickCount64()+10000;while(xideTemplateViewIsOpen(a)&&GetTickCount64()<until){if(!settle(a))return 0;Sleep(5);}return !xideTemplateViewIsOpen(a);}
static int websiteClick(xide_app* a,int releaseOutside){xui_widget list=xideHomeControl(a,XIDE_HOME_TEMPLATES);xui_rect_t world=xuiWidgetGetWorldRect(list),row=xuiListViewGetItemRect(list,0);int x=world.fX+row.fX+row.fW-36,y=world.fY+row.fY+28;return xuiQueryCursor(a->context,x,y)==XUI_CURSOR_HAND&&xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(a->context,releaseOutside ? world.fX+8:x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&settle(a);}
static int buttonFits(xide_app* a,xui_widget widget){xui_vec2_t text={0};const char* label=xuiButtonGetText(widget);return xideDpiTextMeasure(&a->proxy,a->uiFont,label,&text)==XUI_OK&&xuiWidgetGetWorldRect(widget).fW>=ceilf(text.fX)+32*xideUiFontScale(a);}
static int optionBeside(xui_widget input,xui_widget option){xui_rect_t a=xuiWidgetGetWorldRect(input),b=xuiWidgetGetWorldRect(option);return xuiWidgetGetParent(input)==xuiWidgetGetParent(option)&&b.fX>=a.fX+a.fW&&abs((a.fY+a.fH/2)-(b.fY+b.fH/2))<=1&&*xuiCheckBoxGetText(option);}
static int labelCentered(xui_widget input){xui_widget row=xuiWidgetGetParent(xuiWidgetGetParent(input)),label=xuiWidgetGetFirstChild(row);xui_rect_t a=xuiWidgetGetWorldRect(input),b=xuiWidgetGetWorldRect(label);return abs((a.fY+a.fH/2)-(b.fY+b.fH/2))<=1;}
static int activateMenu(xide_app* a,xui_widget menu,int index){return xuiMenuOpenAt(menu,a->menuBar,8,60)==XUI_OK&&xuiMenuSetHoverIndex(menu,index)==XUI_OK&&xuiMenuCommitHover(menu)>=XUI_OK&&settle(a);}
static int key(xide_app* a,int code,uint32_t mods){return xuiInputKeyDownEx(a->context,code,mods,NULL)==XUI_OK&&xuiInputKeyUpEx(a->context,code,mods,NULL)==XUI_OK&&settle(a);}
static int run(void* raw){
    xide_app a={0};xide_model_error error={0};int began=0;char *root=NULL,*settings=NULL,*workspace=NULL;xide_template_registry registry={0};started=1;
    char* id=xideCreateId("template-view");root=id ? xidePathResolve(raw,id):NULL;free(id);CHECK(root&&xideIoCreateDirectories(root));settings=xidePathResolve(root,"settings.json");workspace=xidePathResolve(root,"workspace.xlw");CHECK(settings&&workspace);
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;CHECK(xideAppCreate(&a)==XUI_OK&&xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&a)&&settle(&a));a.hidden=0;
    CHECK(xideActionNamed("file.new")->id==XIDE_ACTION_NEW&&xideActionNamed("file.open")->id==XIDE_ACTION_OPEN&&!xideActionNamed("home.new")&&!xideActionNamed("workspace.new")&&!xideActionNamed("project.open"));
    CHECK(xuiMenuGetItem(a.fileMenu,0)->iValue==XIDE_ACTION_NEW&&xuiMenuGetItem(a.fileMenu,1)->iValue==XIDE_ACTION_OPEN&&xuiMenuGetItem(a.fileMenu,2)->iType==XUI_MENU_ITEM_SUBMENU&&xuiMenuGetItem(a.fileMenu,2)->pSubmenu);
    CHECK(activateMenu(&a,a.fileMenu,0)&&xideDialogIsOpen(xideHomeNewWindow(&a))&&click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)));
    CHECK(click(&a,xideMainToolbarControl(&a,XIDE_TB_NEW))&&xideDialogIsOpen(xideHomeNewWindow(&a))&&click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)));
    CHECK(key(&a,'N',XUI_MOD_CTRL)&&xideDialogIsOpen(xideHomeNewWindow(&a))&&click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)));
    CHECK(activateMenu(&a,a.fileMenu,1)&&xideFileDialogCurrent(&a)&&!strcmp(xuiFileDialogGetFilterPattern(xideFileDialogCurrent(&a),0),"*.xlw;*.xlp;*.xl;*.c"));CHECK(xuiFileDialogCancel(xideFileDialogCurrent(&a))==XUI_OK&&settle(&a));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_NEW))&&xideDialogIsOpen(xideHomeNewWindow(&a)));
    CHECK(xuiTreeViewGetSelected(xideHomeControl(&a,XIDE_HOME_CATEGORY))==2&&xuiListViewGetItemCount(xideHomeControl(&a,XIDE_HOME_TEMPLATES))==4);
    CHECK(strstr(xuiListViewGetItemText(xideHomeControl(&a,XIDE_HOME_TEMPLATES),0),"控制台"));CHECK(xuiListViewGetItemHeight(xideHomeControl(&a,XIDE_HOME_TEMPLATES))>=96*xideUiFontScale(&a));CHECK(shot(&a,raw,"templates-project.png"));
    CHECK(buttonFits(&a,xideHomeControl(&a,XIDE_HOME_CREATE)));CHECK(websiteClick(&a,1)&&!websiteOpens);CHECK(websiteClick(&a,0)&&websiteOpens==1&&!wcscmp(openedWebsite,L"https://xcompiler.com/book/index.html")&&xideDialogIsOpen(xideHomeNewWindow(&a))&&!xideTemplateViewIsOpen(&a));
    CHECK(category(&a,3)&&settle(&a)&&xuiListViewGetItemCount(xideHomeControl(&a,XIDE_HOME_TEMPLATES))==2);CHECK(shot(&a,raw,"templates-files.png"));
    CHECK(buttonFits(&a,xideHomeControl(&a,XIDE_HOME_CREATE)));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL))&&xideActionExecute(&a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED&&settle(&a));CHECK(xuiTreeViewGetSelected(xideHomeControl(&a,XIDE_HOME_CATEGORY))==2);
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CREATE))&&xideTemplateViewIsOpen(&a)&&!xideDialogIsOpen(xideHomeNewWindow(&a)));
    CHECK(xuiComboBoxGetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE))==0&&!xuiComboBoxIsItemEnabled(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE),2));
    CHECK(!xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP)));CHECK(optionBeside(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME),xideTemplateViewControl(&a,XIDE_TEMPLATE_PROJECT_DIRECTORY))&&labelCentered(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME))&&labelCentered(xideTemplateViewControl(&a,XIDE_TEMPLATE_PARENT)));
    CHECK(click(&a,xideTemplateViewControl(&a,XIDE_TEMPLATE_PROJECT_DIRECTORY))&&!xuiCheckBoxGetChecked(xideTemplateViewControl(&a,XIDE_TEMPLATE_PROJECT_DIRECTORY)));CHECK(click(&a,xideTemplateViewControl(&a,XIDE_TEMPLATE_PROJECT_DIRECTORY))&&xuiCheckBoxGetChecked(xideTemplateViewControl(&a,XIDE_TEMPLATE_PROJECT_DIRECTORY)));
    CHECK(!xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME)));CHECK(shot(&a,raw,"project-no-workspace.png"));
    CHECK(xuiComboBoxSetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE),1)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a));CHECK(xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME)));CHECK(shot(&a,raw,"project-new-workspace.png"));
    CHECK(xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP))&&*xuiCheckBoxGetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP)));CHECK(optionBeside(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME),xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY))&&labelCentered(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME)));CHECK(click(&a,xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY))&&!xuiCheckBoxGetChecked(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY)));CHECK(click(&a,xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY))&&xuiCheckBoxGetChecked(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY)));
    CHECK(xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME),"创建测试")==XUI_OK&&settle(&a));CHECK(!strcmp(xuiEditGetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME)),"创建测试"));
    CHECK(xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME),"自选空间")==XUI_OK&&settle(&a));CHECK(xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME),"变更名称")==XUI_OK&&settle(&a));CHECK(!strcmp(xuiEditGetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_NAME)),"自选空间"));
    CHECK(button(&a,XIDE_TEMPLATE_CANCEL)&&!xideTemplateViewIsOpen(&a));CHECK(!a.workspaceModel.source);
    CHECK(xideActionExecute(&a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED&&settle(&a)&&click(&a,xideHomeControl(&a,XIDE_HOME_CREATE))&&xideTemplateViewIsOpen(&a));
    CHECK(xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME),"界面工程")==XUI_OK&&xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_PARENT),root)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a));
    CHECK(button(&a,XIDE_TEMPLATE_CREATE)&&finish(&a)&&a.workspaceModel.projectCount==1&&a.projectSessionPath&&a.activeDocument);CHECK(!a.workspaceModel.manifest);CHECK(xideWorkspaceSaveAll(&a,workspace,&error));
    CHECK(xideActionExecute(&a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED&&settle(&a)&&click(&a,xideHomeControl(&a,XIDE_HOME_CREATE))&&xideTemplateViewIsOpen(&a));CHECK(xuiComboBoxGetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE))==2);
    CHECK(xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP))&&!xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_DIRECTORY)));CHECK(xuiComboBoxSetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE),0)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a)&&!xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP)));CHECK(xuiComboBoxSetSelected(xideTemplateViewControl(&a,XIDE_TEMPLATE_WORKSPACE_MODE),2)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a)&&xuiWidgetGetVisible(xideTemplateViewControl(&a,XIDE_TEMPLATE_STARTUP)));
    CHECK(xuiEditSetText(xideTemplateViewControl(&a,XIDE_TEMPLATE_NAME),"加入当前")==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a));CHECK(shot(&a,raw,"project-current-workspace.png"));CHECK(button(&a,XIDE_TEMPLATE_CREATE)&&finish(&a)&&a.workspaceModel.projectCount==2&&a.workspaceModel.dirty);
    CHECK(xideTemplateRegistryLoad(&registry,NULL,&error));xide_template_definition custom={0};CHECK(xideTemplateDefinitionCopy(&custom,&registry.items[0]));xrtValueRelease(custom.fields);const char* customFields="[{\"id\":\"custom.extra\",\"type\":\"text\",\"label\":\"自定义字段\",\"default\":\"中文值\"},{\"id\":\"custom.count\",\"type\":\"integer\",\"label\":\"数量\",\"default\":3},{\"id\":\"custom.enabled\",\"type\":\"boolean\",\"label\":\"启用\",\"default\":false},{\"id\":\"custom.dependent\",\"type\":\"multiline\",\"label\":\"说明\",\"visibleWhen\":{\"field\":\"custom.enabled\",\"equals\":true}}]";custom.fields=xrtJsonParse((xstrview){customFields,strlen(customFields)});
    free(custom.id);custom.id=malloc(24);CHECK(custom.id);strcpy(custom.id,"example.fields");CHECK(xideTemplateViewOpenDefinition(&a,&custom)&&settle(&a));CHECK(xideTemplateViewField(&a,"custom.extra")&&!strcmp(xuiEditGetText(xideTemplateViewField(&a,"custom.extra")),"中文值"));CHECK(!xuiWidgetGetVisible(xideTemplateViewField(&a,"custom.dependent")));CHECK(xuiCheckBoxSetChecked(xideTemplateViewField(&a,"custom.enabled"),1)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a));CHECK(xuiWidgetGetVisible(xideTemplateViewField(&a,"custom.dependent"))&&xuiTextEditSetText(xideTemplateViewField(&a,"custom.dependent"),"两行\n中文说明")==XUI_OK&&settle(&a));{xui_rect_t area=xuiWidgetGetWorldRect(xideTemplateViewField(&a,"custom.dependent"));CHECK(area.fW>200&&area.fH>=80);CHECK(xuiWidgetGetCachePolicy(xideTemplateViewField(&a,"custom.dependent")).iPolicy!=XUI_CACHE_POLICY_NONE);}CHECK(shot(&a,raw,"project-custom-fields.png"));CHECK(button(&a,XIDE_TEMPLATE_CANCEL));xideTemplateDefinitionClear(&custom);
    CHECK(xideWorkspaceSaveAll(&a,workspace,&error));
    CHECK(xideActionExecute(&a,XIDE_ACTION_VIEW_HOME)==XIDE_ACTION_DISPATCHED&&settle(&a));CHECK(xuiDockPanelHideWindow(a.dock,a.windows[XIDE_HOME])==XUI_OK&&settle(&a)&&!xideWorkbenchWindowVisible(&a,XIDE_HOME));
    CHECK(click(&a,xideMainToolbarControl(&a,XIDE_TB_NEW))&&xideDialogIsOpen(xideHomeNewWindow(&a))&&category(&a,4)&&settle(&a));CHECK(xuiListViewGetItemCount(xideHomeControl(&a,XIDE_HOME_TEMPLATES))==1&&!strcmp(xuiListViewGetItemText(xideHomeControl(&a,XIDE_HOME_TEMPLATES),0),"空白工作空间"));CHECK(shot(&a,raw,"templates-workspace.png"));
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CREATE))&&xideTemplateViewIsOpen(&a)&&!strcmp(xuiWindowGetTitle(xideTemplateViewWindow(&a)),"创建工作空间"));CHECK(!xideTemplateViewField(&a,"project.name")&&!xideTemplateViewField(&a,"workspace.mode")&&xideTemplateViewField(&a,"workspace.location"));
    CHECK(xuiEditSetText(xideTemplateViewField(&a,"workspace.name"),"工程集群")==XUI_OK&&xuiEditSetText(xideTemplateViewField(&a,"workspace.location"),root)==XUI_OK&&xideTemplateViewRefresh(&a)&&settle(&a));CHECK(shot(&a,raw,"create-workspace.png"));
    CHECK(button(&a,XIDE_TEMPLATE_CANCEL));CHECK(xideActionExecute(&a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED&&settle(&a)&&category(&a,4)&&click(&a,xideHomeControl(&a,XIDE_HOME_CREATE)));CHECK(button(&a,XIDE_TEMPLATE_CREATE)&&finish(&a)&&a.workspaceModel.manifest&&a.workspaceModel.projectCount==0&&a.workspaceModel.files.count==0&&!a.projectSessionPath&&!strcmp(a.workspaceModel.name,"工程集群"));
    CHECK(xideIoExists(a.workspaceModel.manifest));
    for(int i=0;i<14;++i){char file[80];snprintf(file,sizeof(file),"history-%d.xl",i);char* path=xidePathResolve(root,file);CHECK(path);xideHomeRemember(&a,path);free(path);}xideHomeRemember(&a,a.workspaceModel.manifest);xideActionsRefresh(&a);
    {xui_widget recent=xuiMenuGetItem(a.fileMenu,2)->pSubmenu;CHECK(xuiMenuGetItemCount(recent)==13&&xuiMenuGetItem(recent,12)->iValue==-1);CHECK(activateMenu(&a,a.fileMenu,2)&&xuiMenuIsOpen(recent));CHECK(xuiMenuSetHoverIndex(recent,0)==XUI_OK&&xuiMenuCommitHover(recent)>=XUI_OK&&settle(&a)&&a.workspaceModel.projectCount==0&&!xideDialogIsOpen(xideHomeNewWindow(&a)));}
    CHECK(key(&a,'O',XUI_MOD_CTRL)&&xideFileDialogCurrent(&a));CHECK(xuiFileDialogCancel(xideFileDialogCurrent(&a))==XUI_OK&&settle(&a));
    CHECK(xideActionExecute(&a,XIDE_ACTION_NEW)==XIDE_ACTION_DISPATCHED&&settle(&a));CHECK(xideUiFontApply(&a,a.uiFontPath,32,&error)&&settle(&a));CHECK(shot(&a,raw,"templates-large-font.png"));
    {xui_rect_t body=xuiWidgetGetWorldRect(xideDialogGetScrollViewWidget(xideHomeNewWindow(&a))),footer=xuiWidgetGetWorldRect(xideDialogGetFooterWidget(xideHomeNewWindow(&a)));CHECK(body.fY+body.fH<=footer.fY+1&&footer.fH>0);}
    CHECK(click(&a,xideHomeControl(&a,XIDE_HOME_CANCEL)));
    CHECK(!a.frameError);printf("template_view_test passed: %d checks, icon rows, fixed categories, defaults, conditional fields, cancellation, real background project creation and current-workspace association\n",checks);
cleanup:if(began)xgeEnd();xideAppDestroy(&a);xideTemplateRegistryClear(&registry);free(root);free(settings);free(workspace);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv){if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="Template catalog verification";d.iFlags=XGE_INIT_WINDOW;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK&&started&&!failed ? 0:1;}
