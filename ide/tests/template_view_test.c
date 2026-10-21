#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/template_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/references_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char* directory; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"template_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideTemplateViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r)
{ float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
 xui_widget w=xideTemplateViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideTemplateViewWindow(a));
 if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),visible=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<visible.fX) { r.fX=visible.fX; } if(r.fY<visible.fY) { r.fY=visible.fY; }
 if(right>visible.fX+visible.fW) { right=visible.fX+visible.fW; } if(bottom>visible.fY+visible.fH) { bottom=visible.fY+visible.fH; }
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int menuNew(xide_app* a)
{
 int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->projectMenu);++i) if(xuiMenuGetItem(a->projectMenu,i)->iValue==XIDE_ACTION_NEW_PROJECT) index=i;
 if(index<0 || xuiMenuOpenAt(a->projectMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(a->projectMenu,index),world=xuiWidgetGetWorldRect(a->projectMenu); r.fX+=world.fX; r.fY+=world.fY;
 return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && settle(a) && xuiMenuGetHoverIndex(a->projectMenu)==index && pointer(a,r);
}
static int choose(xide_app* a,int id,int index)
{
 xui_widget w=xideTemplateViewControl(a,id); if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideTemplateViewWindow(a)),w)!=XUI_OK || !settle(a) || xuiComboBoxOpen(w)!=XUI_OK || !settle(a)) return 0;
 xui_widget menu=xuiComboBoxGetMenuWidget(w); xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY; return pointer(a,r) && xuiComboBoxGetSelected(w)==index;
}
static int fields(xide_app* a,const char* root,const char* name)
{ return xuiEditSetText(xideTemplateViewControl(a,XIDE_TEMPLATE_PARENT),root)==XUI_OK && xuiEditSetText(xideTemplateViewControl(a,XIDE_TEMPLATE_NAME),name)==XUI_OK && xideTemplateViewRefresh(a) && settle(a); }
static int syntaxToken(xui_widget editor,const char* spelling,int kind)
{
 const char* content=xuiCodeEditGetText(editor); const char* word=strstr(content,spelling); if(!word) return 0;
 xui_code_token_t tokens[256]; int count=0,offset=(int)(word-content);
 if(xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,256,&count)!=XUI_OK) return 0;
 for(int i=0;i<count;++i) { if(tokens[i].iStartOffset==offset && tokens[i].iKind==kind) return 1; } return 0;
}
static int capture(xide_app* a,const char* path)
{ if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int resize(xide_app* a,int width,int height)
{ xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface next=NULL; if(a->proxy.surfaceCreate(&a->proxy,&next,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=next; a->viewportWidth=width; a->viewportHeight=height; return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a); }
static xui_widget firstInput(xui_context c,xui_widget w)
{ if(xuiWidgetGetType(w)==xuiWidgetFindType(c,"input")) return w; for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=firstInput(c,child); if(found) return found; } return NULL; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0;
 char *manifestPreview=NULL; unsigned char* manifestBytes=NULL; char *root=NULL,*settings=NULL,*workspace=NULL,*imageXl=NULL,*imageC=NULL,*imageLarge=NULL,*directory=NULL,*startup=NULL,*native=NULL,*settingsBytes=NULL; xvalue* exported=NULL;
 char* unique=xideCreateId("template-ui"); root=unique ? xidePathResolve(f->directory,unique) : NULL; free(unique); CHECK(root && xrtDirCreateAll(root));
 settings=xidePathResolve(root,"settings.json"); workspace=xidePathResolve(root,"workspace.xlw"); imageXl=xidePathResolve(root,"native-template-xlang.png"); imageC=xidePathResolve(root,"native-template-c.png"); imageLarge=xidePathResolve(root,"native-template-large-font.png"); CHECK(settings && workspace && imageXl && imageC && imageLarge);
 app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(!app.workspaceModel.source && xideActionQuery(&app,XIDE_ACTION_NEW_PROJECT).enabled && menuNew(&app) && xideTemplateViewIsOpen(&app));
 CHECK(fields(&app,root,"第一个项目😀") && choose(&app,XIDE_TEMPLATE_KIND,0)); directory=xidePathResolve(root,"第一个项目😀"); CHECK(directory && !xidePathExists(directory));
 CHECK(xuiCheckBoxGetChecked(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP)) && xideTemplateViewOpen(&app) && !strcmp(xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME)),"第一个项目😀"));
 CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideReferencesViewOpen(&app) && !xideConfigurationViewOpen(&app) && !xideFontViewOpen(&app));
 CHECK(click(&app,XIDE_TEMPLATE_CANCEL) && !xideTemplateViewIsOpen(&app) && !app.workspaceModel.source && !xidePathExists(directory));
 CHECK(xideTemplateViewOpen(&app) && click(&app,XIDE_TEMPLATE_CREATE) && !xideTemplateViewIsOpen(&app) && app.workspaceModel.projectCount==1 && app.activeDocument && !strcmp(app.activeDocument->language,"xlang") && !app.activeDocument->dirty && app.workspaceModel.dirty);
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,app.selectedProject); CHECK(p && xidePathEqual(p->entry,app.activeDocument->path) && !strcmp(app.workspaceModel.startupProject,p->id)); startup=malloc(strlen(p->id)+1); CHECK(startup); strcpy(startup,p->id); free(directory); directory=NULL;
 CHECK(xideWorkspaceSaveAll(&app,workspace,&error) && xideTemplateViewOpen(&app) && fields(&app,root,"原生混合工程") && choose(&app,XIDE_TEMPLATE_KIND,2));
 CHECK(!xuiCheckBoxGetChecked(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP)) && !strcmp(xuiCodeEditGetLanguage(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW)),"xlang") && strstr(xuiCodeEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW)),"nativeAnswer") && syntaxToken(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW),"func main",XUI_CODE_TOKEN_KEYWORD) && capture(&app,imageXl));
 CHECK(choose(&app,XIDE_TEMPLATE_FILES,1) && !strcmp(xuiCodeEditGetLanguage(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW)),"c") && strstr(xuiCodeEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW)),"#include") && syntaxToken(xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW),"native_answer",XUI_CODE_TOKEN_IDENTIFIER) && capture(&app,imageC));
 xui_widget preview=xideTemplateViewControl(&app,XIDE_TEMPLATE_PREVIEW); const char* current=xuiCodeEditGetText(preview); char* original=malloc(strlen(current)+1); CHECK(original); strcpy(original,current);
 CHECK(xuiCodeEditIsReadonly(preview) && xuiSetFocusWidget(app.context,preview)==XUI_OK && xuiInputText(app.context,'X')==XUI_OK && settle(&app) && !strcmp(original,xuiCodeEditGetText(preview))); free(original);
 CHECK(choose(&app,XIDE_TEMPLATE_FILES,5) && strstr(xuiCodeEditGetText(preview),"native.c") && strstr(xuiCodeEditGetText(preview),"project-"));
 manifestPreview=malloc(strlen(xuiCodeEditGetText(preview))+1); CHECK(manifestPreview); strcpy(manifestPreview,xuiCodeEditGetText(preview));
 CHECK(click(&app,XIDE_TEMPLATE_CREATE) && app.workspaceModel.projectCount==2 && !strcmp(app.workspaceModel.startupProject,startup)); p=xideWorkspaceFindProject(&app.workspaceModel,app.selectedProject); CHECK(p && p->cSources.count==1 && p->files.count==5);
 size_t manifestLength=0; manifestBytes=xrtFileReadAll(p->manifest,&manifestLength); CHECK(manifestBytes && manifestLength==strlen(manifestPreview) && !memcmp(manifestBytes,manifestPreview,manifestLength)); xrtFree(manifestBytes); manifestBytes=NULL; free(manifestPreview); manifestPreview=NULL;
 directory=xidePathDirectory(p->manifest); native=directory ? xidePathResolve(directory,"native.c") : NULL; free(directory); directory=NULL; CHECK(native && xideEditorOpen(&app,native,&error) && !strcmp(app.activeDocument->language,"c")); xide_document* c=app.activeDocument; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor));
 CHECK(xideTemplateViewOpen(&app) && fields(&app,root,"只取消的项目") && xuiSetFocusWidget(app.context,xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME))==XUI_OK && xuiInputKeyDownEx(app.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,'Z',0,NULL)==XUI_OK && settle(&app) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==version);
 CHECK(xideWorkspaceSetStartup(&app.workspaceModel,app.selectedProject,&error) && !xideTemplateViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideTemplateViewIsOpen(&app) && click(&app,XIDE_TEMPLATE_CANCEL));
 CHECK(xideWorkspaceSetStartup(&app.workspaceModel,startup,&error) && xideWorkspaceSaveAll(&app,NULL,&error));
 CHECK(xideTemplateViewOpen(&app) && fields(&app,root,"基线不可共用") && xrtValueObjectSetNew(app.workspaceModel.source,sv("future"),xrtValueBool(1)) && !xideTemplateViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_TEMPLATE_CANCEL));
 CHECK(xrtValueObjectRemove(app.workspaceModel.source,sv("future")) && xideWorkspaceSaveAll(&app,NULL,&error) && xideEditorOpen(&app,workspace,&error)); xide_document* manifest=app.activeDocument;
 CHECK(xuiCodeEditSetText(manifest->editor,"{\n")==XUI_OK && settle(&app) && manifest->dirty && xideTemplateViewOpen(&app) && fields(&app,root,"清单脏保护") && !xideTemplateViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_TEMPLATE_CANCEL));
 CHECK(xideDocumentReload(manifest,1,&error) && xideEditorShow(&app,c)==XUI_OK && settle(&app)); xui_widget property=firstInput(app.context,xuiScrollViewGetContentWidget(app.properties)); p=xideWorkspaceFindProject(&app.workspaceModel,app.selectedProject); CHECK(property && p);
 CHECK(xuiEditSetText(property,"尚未应用的名称")==XUI_OK && xideWorkspaceViewHasDraft(&app) && !xideActionQuery(&app,XIDE_ACTION_NEW_PROJECT).enabled && !xideTemplateViewOpen(&app)); CHECK(xuiEditSetText(property,p->name)==XUI_OK && !xideWorkspaceViewHasDraft(&app));
 CHECK(xideTemplateViewOpen(&app) && fields(&app,root,"碰撞目录")); directory=xidePathResolve(root,"碰撞目录"); CHECK(directory && xrtDirCreateAll(directory)); size_t count=app.workspaceModel.projectCount; CHECK(!xideTemplateViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && app.workspaceModel.projectCount==count && xideTemplateViewIsOpen(&app) && !strcmp(xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME)),"碰撞目录")); free(directory); directory=NULL;
 CHECK(xuiEditSetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME),"CON")==XUI_OK && !xideTemplateViewRefresh(&app) && !xuiWidgetGetEnabled(xideTemplateViewControl(&app,XIDE_TEMPLATE_CREATE)) && click(&app,XIDE_TEMPLATE_CANCEL));
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error) && xideTemplateViewOpen(&app) && fields(&app,root,"暂缓磁盘协调"));
 exported=xideWorkspaceExport(&app.workspaceModel,&error); CHECK(exported && xrtValueObjectSetNew(exported,sv("name"),xrtValueString(sv("磁盘工作空间")))); size_t n=0; char* disk=xrtJsonStringify(exported,0,&n); xrtValueRelease(exported); exported=NULL; CHECK(disk && writeText(workspace,disk)); xrtFree(disk);
 uint64_t revision=xideFileSyncRevision(&app),until=GetTickCount64()+8000; do { CHECK(settle(&app)); if(xideFileSyncRevision(&app)!=revision && xideFileSyncCount(&app)) break; Sleep(5); } while(GetTickCount64()<until);
 CHECK(xideTemplateViewIsOpen(&app) && strcmp(app.workspaceModel.name,"磁盘工作空间") && xideFileSyncCount(&app) && click(&app,XIDE_TEMPLATE_CANCEL)); until=GetTickCount64()+8000; do { CHECK(settle(&app)); if(!strcmp(app.workspaceModel.name,"磁盘工作空间")) break; Sleep(5); } while(GetTickCount64()<until); CHECK(!strcmp(app.workspaceModel.name,"磁盘工作空间") && c->open && !c->dirty);
 CHECK(xideTemplateViewOpen(&app) && fields(&app,root,"大字体工程") && choose(&app,XIDE_TEMPLATE_KIND,2) && choose(&app,XIDE_TEMPLATE_FILES,1));
 CHECK(xideEditorFontApply(&app,app.codeFontPath,22,&error) && xuiCodeEditGetFont(preview)==app.codeFont && xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,900,600));
 CHECK(xuiInputGetFont(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME))==app.uiFont && xuiCheckBoxGetFont(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP))==app.uiFont && xuiCheckBoxGetIndicatorSize(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP))==45 && xuiCodeEditGetFont(preview)==app.codeFont);
 n=0; settingsBytes=xrtJsonStringify(app.preferences.source,1,&n); CHECK(settingsBytes && writeText(settings,"{\"format\":\"xlang-ide-settings-1\",\"external\":true}")); xui_font old=app.uiFont;
 CHECK(!xideUiFontApply(&app,app.uiFontPath,18,&error) && app.uiFont==old && app.uiFontSize==40 && xuiCheckBoxGetFont(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP))==old && !strcmp(xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME)),"大字体工程")); CHECK(writeText(settings,settingsBytes)); xrtFree(settingsBytes); settingsBytes=NULL;
 CHECK(click(&app,XIDE_TEMPLATE_STARTUP) && xuiCheckBoxGetChecked(xideTemplateViewControl(&app,XIDE_TEMPLATE_STARTUP)) && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideTemplateViewWindow(&app)),xideTemplateViewControl(&app,XIDE_TEMPLATE_CREATE))==XUI_OK && capture(&app,imageLarge));
 CHECK(click(&app,XIDE_TEMPLATE_CREATE) && !xideTemplateViewIsOpen(&app) && app.workspaceModel.projectCount==3 && !strcmp(app.workspaceModel.startupProject,app.selectedProject) && app.activeDocument && !strcmp(app.activeDocument->language,"xlang") && !app.activeDocument->dirty && c->open && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(c->editor))==version);
 CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && xideEditorFontApply(&app,app.codeFontPath,16,&error) && resize(&app,1280,800) && xideWorkspaceSaveAll(&app,NULL,&error));
 puts("template_view_test passed: empty-workbench and registered action creation, native template/file selection, readonly C/xlang/manifest previews, exact draft/cancel, auto entry and tree selection, startup default/preservation/explicit checkbox, document/plain-field/modal isolation, independent stale model and dirty manifest/property protection, collision/invalid-name states, actual file-sync deferral/resume, code/UI fonts/rollback, 125% and 40 DIP two-axis native create, three screenshots.");
cleanup:
 if(began) { xgeEnd(); } xideAppDestroy(&app); free(manifestPreview); xrtFree(manifestBytes); xrtValueRelease(exported); xrtFree(settingsBytes); free(root); free(settings); free(workspace); free(imageXl); free(imageC); free(imageLarge); free(directory); free(startup); free(native); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang project-template verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{ if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1]}; wchar_t name[100]; swprintf(name,100,L"XlangTemplateVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }