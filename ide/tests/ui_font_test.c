#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "ui/font_view.h"
#include "ui/editor_view.h"
#include "ui/catalog_view.h"
#include "ui/workspace_view.h"
#include "ui/configuration_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"ui_font_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture { const char* directory; HDESK desktop; int started,failed; } fixture;
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static void errorDetails(xui_context context,const xui_error_info_t* e,void* user)
{
    (void)context; xide_app* app=user; app->frameError=e->iCode;
    const char* type=e->pWidget ? xuiWidgetTypeGetName(xuiWidgetGetType(e->pWidget)) : "none";
    xui_rect_t rect=e->pWidget ? xuiWidgetGetRect(e->pWidget) : (xui_rect_t){0};
    xui_font f=NULL; const char* text="";
    if(e->pWidget && !strcmp(type,"label")) { f=xuiLabelGetFont(e->pWidget); text=xuiLabelGetText(e->pWidget); }
    fprintf(stderr,"UI error %d %s type=%s font=%p UI=%p rect=%d,%d,%d,%d text=%s\n",e->iCode,e->sOperation,type,(void*)f,(void*)app->uiFont,rect.fX,rect.fY,rect.fW,rect.fH,text);
}
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app);
    if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideCatalogViewLayout(app); xideWorkspaceViewLayout(app);
    return xuiLayout(app->context)==XUI_OK && !app->frameError;
}
static int clickPopup(xide_app* app,xui_widget popupWidget,xui_widget widget)
{
    if(!settle(app)) return 0;
    xui_widget scroll=xideDialogGetScrollViewWidget(popupWidget);
    if(!scroll || xuiScrollViewEnsureChildVisible(scroll,widget)!=XUI_OK || !settle(app)) return 0;
    xui_rect_t r=xuiWidgetGetWorldRect(widget),popup=xuiWidgetGetWorldRect(popupWidget);
    int x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    if(x<popup.fX || y<popup.fY || x>=popup.fX+popup.fW || y>=popup.fY+popup.fH) return 0;
    return xuiInputPointerMove(app->context,x,y,0)==XUI_OK && xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK &&
        xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(app);
}
static int click(xide_app* app,int control) { return clickPopup(app,xideFontViewWindow(app),xideFontViewControl(app,control)); }
static int propertyFields(xide_app* app,xui_widget widget)
{
    int count=0; xui_widget_type type=xuiWidgetGetType(widget);
    if(type==xuiWidgetFindType(app->context,"input")) { if(xuiInputGetFont(widget)!=app->uiFont) return -100; ++count; }
    if(type==xuiWidgetFindType(app->context,"textedit")) { if(xuiTextEditGetFont(widget)!=app->uiFont) return -100; ++count; }
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) { int result=propertyFields(app,child); if(result<0) return result; count+=result; }
    return count;
}
static int capture(xide_app* app,const char* path)
{
    xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};
    return xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK &&
        xuiRender(app->context,app->target,&rect,1)==XUI_OK && xideAppCapture(app,path)==XUI_OK;
}
static int bound(xide_app* app)
{
    if(xuiGetDefaultFont(app->context)!=app->uiFont || xuiMenuBarGetFont(app->menuBar)!=app->uiFont || xuiToolbarGetFont(app->toolbar)!=app->uiFont ||
       xuiStatusBarGetFont(app->statusBar)!=app->uiFont || xuiDockPanelGetFont(app->dock)!=app->uiFont || xuiTreeViewGetFont(app->commands)!=app->uiFont ||
       xuiTreeViewGetFont(app->workspace)!=app->uiFont || xuiLabelGetFont(app->helpTitle)!=app->uiFont || xuiCodeEditGetFont(app->helpExample)!=app->codeFont ||
       xuiCodeEditGetFont(app->panels[XIDE_OUTPUT])!=app->codeFont || xuiTerminalGetFont(app->panels[XIDE_TERMINAL])!=app->codeFont) return 0;
    for(int i=0;i<xuiMenuBarGetItemCount(app->menuBar);++i) { const xui_menubar_item_t* item=xuiMenuBarGetItem(app->menuBar,i); if(item && item->pMenu && xuiMenuGetFont(item->pMenu)!=app->uiFont) return 0; }
    float row=0; if(xuiTreeViewGetMetrics(app->commands,&row,NULL,NULL,NULL)!=XUI_OK || row<xideUiHeight(app,26)-0.01f) return 0;
    xui_font_metrics_t logical={0},physical={0};
    return app->proxy.fontGetMetrics(&app->proxy,app->uiFont,&logical)==XUI_OK && xideDpiFontPixelMetrics(app->dpiProxy,app->uiFont,&physical)==XUI_OK && fabsf(physical.fSize-logical.fSize*app->dpiScale)<0.01f;
}
static int documents(xide_app* app,xide_document* c,xide_document* xl,uint32_t cv,uint32_t xv,xui_code_selection_t selected,xui_font code)
{
    xui_code_selection_t now={0}; now.iSize=sizeof(now);
    return app->codeFont==code && xuiCodeEditGetFont(c->editor)==code && xuiCodeEditGetFont(xl->editor)==code && app->documents.font==code &&
        xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==cv && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor))==xv &&
        xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&now)==XUI_OK && now.iAnchorOffset==selected.iAnchorOffset && now.iCaretOffset==selected.iCaretOffset &&
        !strcmp(c->language,"c") && !strcmp(xl->language,"xlang");
}
static int verify(void* user)
{
    fixture* f=user; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_settings loaded={0}; xide_project created={0}; int began=0;
    char *root=NULL,*settings=NULL,*font=NULL,*winFont=NULL,*codePath=NULL,*bad=NULL,*image=NULL,*modalImage=NULL,*baseline=NULL,*entry=NULL,*projectPath=NULL; unsigned char* bytes=NULL; HANDLE locked=INVALID_HANDLE_VALUE;
    root=xidePathResolve(f->directory,"中文 界面字体"); CHECK(root && xrtDirCreateAll(root));
    settings=xidePathResolve(root,"settings.json"); font=xidePathResolve(root,"界面 字体.ttc"); winFont=xideUiFontDefaultPath(); codePath=xideWindowsFontPath("consola.ttf");
    bad=xidePathResolve(root,"bad.ttf"); image=xidePathResolve(root,"native-ui-font.png"); modalImage=xidePathResolve(root,"native-ui-font-modal.png");
    CHECK(settings && font && winFont && codePath && bad && image && modalImage);
    size_t count=0; bytes=xrtFileReadAllLimit(winFont,64u*1024u*1024u,&count); CHECK(bytes && xrtFileWriteAtomic(font,(xbytesview){bytes,count})); xrtFree(bytes); bytes=NULL;
    CHECK(writeText(bad,"not a font") && writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"future\":7,\"ui\":{\"futureUi\":true,\"font\":{\"futureFont\":11}}}"));
    app.hidden=1; app.noSession=1; app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && app.uiFontSize==16 && xgeBegin()==XGE_OK); began=1;
    CHECK(xuiSetErrorCallback(app.context,errorDetails,&app)==XUI_OK);
    entry=xidePathResolve(root,"main.xl"); projectPath=xidePathResolve(root,"font-project.xlp");
    CHECK(entry && projectPath && writeText(entry,"func main(...) as int\n    return 0\nend\n"));
    CHECK(xideProjectCreate(&created,projectPath,"字体项目",entry,&error) && xideProjectSave(&created,NULL,&error));
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"字体工作空间",&error) && xideWorkspaceAddProject(&app.workspaceModel,projectPath,"alpha",&error));
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"alpha") && settle(&app));
    CHECK(xideEditorFontApply(&app,codePath,20,&error)); xui_font code=app.codeFont;
    xide_document* c=xideDocumentNew(&app.documents,"c",&error); xide_document* xl=xideDocumentNew(&app.documents,"xlang",&error); CHECK(c && xl);
    char text[10000]={0}; for(int i=0;i<100;++i) { char row[90]; snprintf(row,sizeof(row),"/* 中文 stable line %d */ int v%d = %d;\n",i,i,i); strcat(text,row); }
    CHECK(xuiCodeEditSetText(c->editor,text)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// undo\n")==XUI_OK);
    CHECK(xuiCodeEditSetText(xl->editor,"func main(...) as int\n    int 中文 = 3\n    return 中文\nend\n")==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"// undo\n")==XUI_OK);
    CHECK(xideEditorShow(&app,xl)==XUI_OK && xideEditorShow(&app,c)==XUI_OK && settle(&app));
    xui_code_selection_t selected={0}; selected.iSize=sizeof(selected); CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&selected)==XUI_OK);
    uint32_t cv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor)),xv=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xl->editor));
    xideDispatchAction(&app,XIDE_ACTION_UI_FONT); CHECK(xideFontViewIsOpen(&app));
    CHECK(xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_PATH),font)==XUI_OK && xuiEditSetText(xideFontViewControl(&app,XIDE_FONT_SIZE),"32")==XUI_OK && click(&app,XIDE_FONT_APPLY));
    CHECK(!xideFontViewIsOpen(&app) && app.uiFontSize==32 && bound(&app) && documents(&app,c,xl,cv,xv,selected,code));
    CHECK(xuiMenuBarGetFont(app.menuBar)==app.uiFont && xuiWidgetGetRect(app.menuBar).fH>=56 && app.codeFontSize==20);
    CHECK(xideSettingsLoad(&loaded,settings,&error) && loaded.uiFontSize==32 && loaded.editorFontSize==20 && xidePathEqual(loaded.uiFont,font) && xidePathEqual(loaded.editorFont,codePath));
    xvalue* ui=xrtValueObjectGet(loaded.source,sv("ui")); CHECK(xrtValueObjectGet(ui,sv("futureUi")) && xrtValueObjectGet(xrtValueObjectGet(ui,sv("font")),sv("futureFont")));
    CHECK(xideFontViewOpenUi(&app) && click(&app,XIDE_FONT_RESET) && app.uiFontSize==32 && !strcmp(xuiEditGetText(xideFontViewControl(&app,XIDE_FONT_SIZE)),"16") && click(&app,XIDE_FONT_CANCEL));
    xui_font old=app.uiFont; CHECK(!xideUiFontApply(&app,bad,20,&error) && error.code==XIDE_MODEL_INVALID && app.uiFont==old);
    CHECK(!xideUiFontApply(&app,font,41,&error) && app.uiFont==old && bound(&app));
    CHECK(xuiCodeEditSetScroll(c->editor,0,100000)==XUI_OK); float sx=0,sy=0,newX=0,newY=0; CHECK(xuiCodeEditGetScroll(c->editor,&sx,&sy)==XUI_OK && sy>200);
    size_t length=0; baseline=xrtJsonStringify(app.preferences.source,true,&length); CHECK(baseline && writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}"));
    CHECK(!xideUiFontApply(&app,font,8,&error) && error.code==XIDE_MODEL_CONFLICT && app.uiFont==old && app.uiFontSize==32 && bound(&app));
    CHECK(xuiCodeEditGetScroll(c->editor,&newX,&newY)==XUI_OK && fabsf(newX-sx)<0.01f && fabsf(newY-sy)<0.01f && documents(&app,c,xl,cv,xv,selected,code));
    CHECK(xrtFileWriteAtomic(settings,(xbytesview){(unsigned char*)baseline,length})); xrtFree(baseline); baseline=NULL;
    wchar_t wide[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,settings,-1,wide,32768)); locked=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideUiFontApply(&app,font,8,&error) && error.code==XIDE_MODEL_IO && app.uiFont==old && bound(&app)); CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(xuiCodeEditGetScroll(c->editor,&newX,&newY)==XUI_OK && fabsf(newY-sy)<0.01f);
    CHECK(propertyFields(&app,xuiScrollViewGetContentWidget(app.properties))>=3);
    CHECK(xideConfigurationViewOpen(&app) && settle(&app));
    CHECK(xuiInputGetFont(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME))==app.uiFont &&
        xuiListViewGetFont(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==app.uiFont &&
        xuiMenuGetFont(xuiComboBoxGetMenuWidget(xideConfigurationViewControl(&app,XIDE_CONFIG_PICKER)))==app.uiFont);
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME),"保留字体更新时的配置草稿")==XUI_OK);
    CHECK(xideUiFontApply(&app,font,40,&error) && settle(&app) && bound(&app) && documents(&app,c,xl,cv,xv,selected,code));
    CHECK(xuiInputGetFont(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME))==app.uiFont &&
        !strcmp(xuiEditGetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME)),"保留字体更新时的配置草稿"));
    float configW=0,configH=0; CHECK(xideDialogGetContentSize(xideConfigurationViewWindow(&app),&configW,&configH)==XUI_OK && configW>app.viewportWidth && configH>app.viewportHeight);
    CHECK(clickPopup(&app,xideConfigurationViewWindow(&app),xideConfigurationViewControl(&app,XIDE_CONFIG_CANCEL)) && !xideConfigurationViewIsOpen(&app));
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"alpha")->name,"字体项目") && propertyFields(&app,xuiScrollViewGetContentWidget(app.properties))>=3);
    CHECK(xideFontViewOpenUi(&app) && settle(&app) && capture(&app,modalImage));
    float contentW=0,contentH=0; CHECK(xideDialogGetContentSize(xideFontViewWindow(&app),&contentW,&contentH)==XUI_OK && contentW>app.viewportWidth && contentH>app.viewportHeight);
    CHECK(click(&app,XIDE_FONT_CANCEL) && !xideFontViewIsOpen(&app));
    CHECK(xideFontViewOpen(&app) && xuiCodeEditGetFont(xideFontViewControl(&app,XIDE_FONT_PREVIEW))==code && click(&app,XIDE_FONT_CANCEL));
    CHECK(xideUiFontApply(&app,font,24,&error) && settle(&app) && bound(&app) && capture(&app,image));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !strstr(xuiCodeEditGetText(c->editor),"// undo"));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && !strstr(xuiCodeEditGetText(xl->editor),"// undo"));
    CHECK(xgeEnd()==XGE_OK); began=0; xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.noSession=1; app.maxFrames=10000; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && app.uiFontSize==24 && app.codeFontSize==20 && xidePathEqual(app.uiFontPath,font) && xidePathEqual(app.codeFontPath,codePath) && bound(&app));
    puts("ui_font_test passed: real Unicode TTF/TTC, native apply/reset/cancel, separate UI/code preferences and physical font sizes at 125 percent, menus/dock/trees/help/output/terminal, project properties and configuration drafts, native popup scrolling at size 40, C/xlang versions/selection/undo preservation, malformed/range rejection, conflict/locked atomic save and reading-position rollback, unknown UI fields, independent restart and native screenshots.");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    if(began) { xgeEnd(); } xideAppDestroy(&app); xideSettingsClear(&loaded); xideProjectClear(&created); free(entry); free(projectPath); xrtFree(bytes); xrtFree(baseline);
    free(root); free(settings); free(font); free(winFont); free(codePath); free(bad); free(image); free(modalImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1;
    xge_desc_t desc={0}; desc.iWidth=1600; desc.iHeight=1000; desc.sTitle="xlang IDE UI font verification"; desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={0}; f.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangIDEUiFontVerification-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0) { fputs("ui_font_test timed out.\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
    CloseDesktop(f.desktop); return (int)code;
}
