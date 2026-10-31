#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/template_view.h"
#include "ui/font_view.h"
#include "ui/references_view.h"
#include "ui/import_view.h"
#include "ui/resource_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char* directory;HDESK desktop;int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks;if(!(e)) { fprintf(stderr,"file_dialog_test:%d: %s; %s\n",__LINE__,#e,error.message);f->failed=1;goto cleanup; } } while(0)
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0;xideFileDialogPoll(a);xideMessagePoll(a);xideEditorPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK; }
static int click(xide_app* a,xui_widget w) { xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int key(xide_app* a,int k) { return xuiInputKeyDownEx(a->context,k,0,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int choose(xide_app* a,const char* path) { xui_file_dialog p=xideFileDialogCurrent(a);char* dir=xidePathDirectory(path);int okay=p && dir && xuiFileDialogSetDirectory(p,dir)==XUI_OK;free(dir);if(!okay) return 0;for(int i=0;i<xuiFileDialogGetEntryCount(p);++i) if(xidePathEqual(path,xuiFileDialogGetEntryPath(p,i))) return xuiFileDialogSelectIndex(p,i)==XUI_OK && settle(a) && click(a,xuiFileDialogGetOkButtonWidget(p));return 0; }
static int capture(xide_app* a,const char* path) { xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static char* copy(const char* s) { size_t n=strlen(s)+1;char* p=malloc(n);if(p) memcpy(p,s,n);return p; }
static int verify(void* user) {
    fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_project project={0};xide_model_error error={0};xide_text_file disk={0};int began=0;
    char *root=NULL,*settings=NULL,*manifest=NULL,*workspace=NULL,*entry=NULL,*source=NULL,*saved=NULL,*stale=NULL,*image=NULL;
    root=xidePathResolve(f->directory,"选择 中文 空格😀");CHECK(root && xrtDirCreateAll(root));settings=xidePathResolve(root,"settings.json");manifest=xidePathResolve(root,"已有项目.xlp");workspace=xidePathResolve(root,"工作空间.xlw");entry=xidePathResolve(root,"main.xl");source=xidePathResolve(root,"源文件😀.c");saved=xidePathResolve(root,"另存为😀.c");stale=xidePathResolve(root,"过期选择.c");
    CHECK(settings && manifest && workspace && entry && source && saved && stale && writeText(entry,"func main(...) as int\nreturn 0\nend\n") && writeText(source,"int original = 1;\n"));
    CHECK(xideProjectCreate(&project,manifest,"已有项目",entry,&error) && xideProjectSave(&project,NULL,&error));
    app.noSession=1;app.settingsPath=settings;CHECK(xgeBegin()==XGE_OK);began=1;CHECK(xideAppCreate(&app)==XUI_OK);
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"选择测试",&error) && xideWorkspaceSave(&app.workspaceModel,workspace,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && settle(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ADD_PROJECT)==XIDE_ACTION_DISPATCHED && settle(&app) && xideFileDialogBusy(&app) && xideMessagePending(&app));
    xui_file_dialog p=xideFileDialogCurrent(&app);CHECK(p && xuiWidgetIsType(xuiFileDialogGetWindowWidget(p),xuiWidgetFindType(app.context,"window")) && xuiFileDialogGetFilterCount(p)==2 && app.workspaceModel.projectCount==0);
    CHECK(!xideActionQuery(&app,XIDE_ACTION_OPEN_FILE).enabled && !xgeRequestQuit());
    image=xidePathResolve(root,"open-project.png");CHECK(image && capture(&app,image));
    CHECK(choose(&app,manifest) && !xideFileDialogBusy(&app) && !xideMessageBusy(&app) && app.workspaceModel.projectCount==1 && xidePathEqual(app.workspaceModel.projects[0].manifest,manifest));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ADD_PROJECT)==XIDE_ACTION_DISPATCHED && settle(&app));p=xideFileDialogCurrent(&app);CHECK(p && click(&app,xuiFileDialogGetCancelButtonWidget(p)) && !xideFileDialogBusy(&app) && !xideMessageBusy(&app) && app.workspaceModel.projectCount==1);
    CHECK(xideActionExecute(&app,XIDE_ACTION_ADD_PROJECT)==XIDE_ACTION_DISPATCHED && settle(&app));free(app.selectedProject);app.selectedProject=copy(app.workspaceModel.projects[0].id);CHECK(app.selectedProject && choose(&app,manifest) && !xideMessageBusy(&app) && app.workspaceModel.projectCount==1);
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,app.workspaceModel.projects[0].id) && !xideWorkspaceViewHasDraft(&app) && settle(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_OPEN_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && choose(&app,source));xide_document* d=app.activeDocument;CHECK(d && !strcmp(d->language,"c") && xidePathEqual(d->path,source));
    CHECK(xuiCodeEditSetText(d->editor,"int preserved = 2;\n")==XUI_OK && settle(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_SAVE_FILE_AS)==XIDE_ACTION_DISPATCHED && settle(&app));p=xideFileDialogCurrent(&app);CHECK(p && xuiInputSetText(xuiFileDialogGetNameInputWidget(p),"另存为😀")==XUI_OK && click(&app,xuiFileDialogGetOkButtonWidget(p)));
    CHECK(!xideMessageBusy(&app) && !d->dirty && xidePathEqual(d->path,saved) && xideTextFileRead(saved,&disk,&error) && !strcmp(disk.text,"int preserved = 2;\n"));xideTextFileClear(&disk);
    CHECK(xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int original = 1;\n"));xideTextFileClear(&disk);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SAVE_FILE_AS)==XIDE_ACTION_DISPATCHED && settle(&app));p=xideFileDialogCurrent(&app);CHECK(p && xuiInputSetText(xuiFileDialogGetNameInputWidget(p),"过期选择")==XUI_OK && xuiCodeEditSetText(d->editor,"int newer = 3;\n")==XUI_OK && click(&app,xuiFileDialogGetOkButtonWidget(p)) && !xideMessageBusy(&app) && !xidePathExists(stale) && xidePathEqual(d->path,saved) && d->dirty);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SAVE_FILE_AS)==XIDE_ACTION_DISPATCHED && settle(&app));p=xideFileDialogCurrent(&app);CHECK(p && xuiInputSetText(xuiFileDialogGetNameInputWidget(p),"另存为😀.c")==XUI_OK && click(&app,xuiFileDialogGetOkButtonWidget(p)));xui_msgbox box=xuiFileDialogGetOverwriteMsgBox(p);CHECK(box && xuiMsgBoxIsOpen(box) && click(&app,xuiMsgBoxGetButtonWidget(box,1)) && xideFileDialogBusy(&app));
    CHECK(click(&app,xuiFileDialogGetOkButtonWidget(p)));box=xuiFileDialogGetOverwriteMsgBox(p);CHECK(box && xuiMsgBoxIsOpen(box) && click(&app,xuiMsgBoxGetButtonWidget(box,0)) && !xideMessageBusy(&app) && !d->dirty);
    CHECK(xideTemplateViewOpen(&app) && settle(&app) && xideTemplateViewIsOpen(&app));const char* parentBefore=xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_PARENT));char* originalParent=copy(parentBefore);CHECK(originalParent && click(&app,xideTemplateViewControl(&app,XIDE_TEMPLATE_BROWSE)) && xideFileDialogBusy(&app));
    CHECK(key(&app,XUI_KEY_ESCAPE) && xideTemplateViewIsOpen(&app) && !strcmp(originalParent,xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_PARENT))));free(originalParent);
    CHECK(click(&app,xideTemplateViewControl(&app,XIDE_TEMPLATE_BROWSE)));p=xideFileDialogCurrent(&app);CHECK(p && xuiFileDialogSetDirectory(p,root)==XUI_OK && settle(&app) && click(&app,xuiFileDialogGetOkButtonWidget(p)) && xideTemplateViewIsOpen(&app) && xidePathEqual(root,xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_PARENT))));
    CHECK(click(&app,xideTemplateViewControl(&app,XIDE_TEMPLATE_CANCEL)) && xideFontViewOpen(&app) && settle(&app) && click(&app,xideFontViewControl(&app,XIDE_FONT_BROWSE)));p=xideFileDialogCurrent(&app);CHECK(p && xuiFileDialogGetFilterCount(p)==1 && key(&app,XUI_KEY_ESCAPE) && xideFontViewIsOpen(&app));CHECK(click(&app,xideFontViewControl(&app,XIDE_FONT_CANCEL)));
    CHECK(xideReferencesViewOpen(&app) && settle(&app) && click(&app,xideReferencesViewControl(&app,XIDE_REF_BROWSE)) && choose(&app,source) && xidePathEqual(source,xuiEditGetText(xideReferencesViewControl(&app,XIDE_REF_VALUE))) && click(&app,xideReferencesViewControl(&app,XIDE_REF_CANCEL)));
    CHECK(xideImportViewOpen(&app) && settle(&app) && click(&app,xideImportViewControl(&app,XIDE_IMPORT_BROWSE_SOURCE)) && choose(&app,workspace) && xidePathEqual(workspace,xuiEditGetText(xideImportViewControl(&app,XIDE_IMPORT_SOURCE))));
    CHECK(click(&app,xideImportViewControl(&app,XIDE_IMPORT_BROWSE_DESTINATION)));p=xideFileDialogCurrent(&app);CHECK(p && xuiFileDialogSetDirectory(p,root)==XUI_OK && settle(&app) && click(&app,xuiFileDialogGetOkButtonWidget(p)));char* importPath=xidePathResolve(root,"legacy-import");CHECK(importPath && xidePathEqual(importPath,xuiEditGetText(xideImportViewControl(&app,XIDE_IMPORT_DESTINATION))) && !xidePathExists(importPath));CHECK(key(&app,XUI_KEY_ESCAPE) && !xideImportViewIsOpen(&app));free(importPath);
    CHECK(xideResourceViewOpenNew(&app) && settle(&app) && click(&app,xideResourceViewControl(&app,XIDE_RES_NEW_BROWSE)));p=xideFileDialogCurrent(&app);const char* resourceName=p && strstr(xuiFileDialogGetFilterPattern(p,0),"xlon") ? "新资源.xlon" : "新资源.json";CHECK(p && xuiFileDialogSetDirectory(p,root)==XUI_OK && xuiInputSetText(xuiFileDialogGetNameInputWidget(p),"新资源")==XUI_OK && settle(&app) && click(&app,xuiFileDialogGetOkButtonWidget(p)));char* resourcePath=xidePathResolve(root,resourceName);CHECK(resourcePath && xidePathEqual(resourcePath,xuiEditGetText(xideResourceViewControl(&app,XIDE_RES_NEW_PATH))) && !xidePathExists(resourcePath) && key(&app,XUI_KEY_ESCAPE) && !xideResourceViewIsOpen(&app));free(resourcePath);
    wchar_t executable[32768];CHECK(GetModuleFileNameW(NULL,executable,32768)>0);int n=WideCharToMultiByte(CP_UTF8,0,executable,-1,NULL,0,NULL,NULL);char* binary=n ? malloc((size_t)n) : NULL;CHECK(binary && WideCharToMultiByte(CP_UTF8,0,executable,-1,binary,n,NULL,NULL));
    CHECK(xideActionExecute(&app,XIDE_ACTION_COMPILER)==XIDE_ACTION_DISPATCHED && settle(&app) && choose(&app,binary) && app.preferences.compiler && xidePathEqual(app.preferences.compiler,binary));
#if XIDE_ENABLE_C_LSP
    CHECK(xideActionExecute(&app,XIDE_ACTION_C_SERVER)==XIDE_ACTION_DISPATCHED && settle(&app) && choose(&app,binary) && app.preferences.cServer && xidePathEqual(app.preferences.cServer,binary));
#else
    CHECK(!xideActionFind(XIDE_ACTION_C_SERVER) && !app.cCompletionView && !app.cLanguageView);
#endif
    free(binary);
    char* font=xideUiFontDefaultPath();CHECK(font && xideUiFontApply(&app,font,40,&error));free(font);CHECK(xideActionExecute(&app,XIDE_ACTION_OPEN_FILE)==XIDE_ACTION_DISPATCHED && settle(&app));p=xideFileDialogCurrent(&app);CHECK(p && xuiFileDialogSetDirectory(p,root)==XUI_OK && settle(&app));free(image);image=xidePathResolve(root,"large-font.png");CHECK(image && capture(&app,image));
    xui_widget combo=xuiFileDialogGetFilterComboWidget(p);CHECK(xuiComboBoxOpen(combo)==XUI_OK && settle(&app));xui_widget menu=xuiComboBoxGetMenuWidget(combo);xui_rect_t item=xuiMenuGetItemRect(menu,1),base=xuiWidgetGetWorldRect(menu);CHECK(xuiInputPointerMove(app.context,base.fX+item.fX+10,base.fY+item.fY+item.fH/2,0)==XUI_OK && xuiInputPointerDown(app.context,base.fX+item.fX+10,base.fY+item.fY+item.fH/2,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app.context,base.fX+item.fX+10,base.fY+item.fY+item.fH/2,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&app) && xuiComboBoxGetSelected(combo)==1);
    CHECK(key(&app,XUI_KEY_ESCAPE) && !xideFileDialogBusy(&app) && !xideMessageBusy(&app));
    printf("file_dialog_test passed: %d checks; real XUI add-existing-project/open/save-as/default-extension/overwrite yes-no, UTF-8 paths, cancelled and stale selection, nested font/folder draft retention, modal quit/actions, 40-DIP filter selection and two renders.\n",f->checks);
cleanup:
    xideTextFileClear(&disk);if(began) xgeEnd();xideAppDestroy(&app);xideProjectClear(&project);free(root);free(settings);free(manifest);free(workspace);free(entry);free(source);free(saved);free(stale);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user) { fixture* f=user;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="XUI file selection regression";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK) r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangFileDialogVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread); }CloseDesktop(f.desktop);return (int)code; }
