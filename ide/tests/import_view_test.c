#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/references_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/reading_state.h"
#include "ui/task_view.h"
#include "services/session.h"
#include "services/file_sync.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"import_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,xideImportViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static char* copy(const char* s) { char* p=malloc(strlen(s)+1); if(p) strcpy(p,s); return p; }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r)
{ float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
 xui_widget w=xideImportViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideImportViewWindow(a));
 if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),visible=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<visible.fX) { r.fX=visible.fX; } if(r.fY<visible.fY) { r.fY=visible.fY; }
 if(right>visible.fX+visible.fW) { right=visible.fX+visible.fW; } if(bottom>visible.fY+visible.fH) { bottom=visible.fY+visible.fH; }
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int menuNew(xide_app* a)
{
 int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->projectMenu);++i) if(xuiMenuGetItem(a->projectMenu,i)->iValue==XIDE_ACTION_IMPORT_LEGACY) index=i;
 if(index<0 || xuiMenuOpenAt(a->projectMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiMenuGetItemRect(a->projectMenu,index),world=xuiWidgetGetWorldRect(a->projectMenu); r.fX+=world.fX; r.fY+=world.fY;
 return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && settle(a) && xuiMenuGetHoverIndex(a->projectMenu)==index && pointer(a,r);
}
static int choose(xide_app* a,int id,int index)
{
 xui_widget w=xideImportViewControl(a,id); if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideImportViewWindow(a)),w)!=XUI_OK || !settle(a) || xuiComboBoxOpen(w)!=XUI_OK || !settle(a)) return 0;
 xui_widget menu=xuiComboBoxGetMenuWidget(w); xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY; return pointer(a,r) && xuiComboBoxGetSelected(w)==index;
}
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
static int fields(xide_app* a,const char* source,const char* destination)
{ return xuiEditSetText(xideImportViewControl(a,XIDE_IMPORT_SOURCE),source)==XUI_OK && xuiEditSetText(xideImportViewControl(a,XIDE_IMPORT_DESTINATION),destination)==XUI_OK && settle(a) && click(a,XIDE_IMPORT_ANALYZE); }
static int reviewed(xide_app* a)
{ return click(a,XIDE_IMPORT_REVIEWED) && xuiCheckBoxGetChecked(xideImportViewControl(a,XIDE_IMPORT_REVIEWED)); }
static int choices(xide_app* a)
{ return choose(a,XIDE_IMPORT_CANDIDATES,0) && choose(a,XIDE_IMPORT_KIND,1) && choose(a,XIDE_IMPORT_STARTUP,1) && reviewed(a); }
static size_t opened(xide_app* a) { size_t n=0; for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open) ++n; return n; }
static int expanded(xide_app* a,const char* kind,const char* project,const char* group,int value)
{
 char* wanted=xideReadingKey(kind,project,group); int okay=0;
 for(int i=0;i<xuiTreeViewGetNodeCount(a->workspace);++i) { const xui_tree_view_node_t* node=xuiTreeViewGetNode(a->workspace,i); char* key=xideWorkspaceTreeKey(a,node->iId); if(key && wanted && !strcmp(key,wanted)) okay=(xuiTreeViewGetNodeExpanded(a->workspace,node->iId)!=0)==value; xrtFree(key); }
 xrtFree(wanted); return okay;
}
static HANDLE lockFile(const char* path)
{ wchar_t wide[32768]; if(!MultiByteToWideChar(CP_UTF8,0,path,-1,wide,32768)) return INVALID_HANDLE_VALUE; return CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); }
static int taskWait(xide_app* a,xide_task* t,xide_task_info* info)
{ uint64_t until=GetTickCount64()+30000; do { xideTasksPoll(a->tasks); xideTaskInfo(t,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(10); } while(GetTickCount64()<until); return 0; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1; xide_app app={0}; xide_model_error error={0}; int began=0; HANDLE lock=INVALID_HANDLE_VALUE;
 char* owned[64]={0}; size_t ownedCount=0; xvalue *hints=NULL,*bad=NULL,*saved=NULL; char *oldBytes=NULL,*oldProjectBytes=NULL,*sessionBytes=NULL; size_t oldLength=0,oldProjectLength=0,sessionLength=0;
#define KEEP(s) (owned[ownedCount++]=(s))
 char* id=xideCreateId("legacy-native"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id); CHECK(root && xrtDirCreateAll(root));
 char *settings=KEEP(xidePathResolve(root,"settings.json")),*sessions=KEEP(xidePathResolve(root,"native-sessions")),*source=KEEP(xidePathResolve(root,"旧 工作空间😀.xlw")),*project=KEEP(xidePathResolve(root,"old.xlp")),*xlPath=KEEP(xidePathResolve(root,"main.xl")),*cPath=KEEP(xidePathResolve(root,"native.c")),*otherXl=KEEP(xidePathResolve(root,"alternate.xl")),*missing=KEEP(xidePathResolve(root,"missing.txt")),*outside=KEEP(xidePathResolve(root,"outside.c"));
 char *target=KEEP(xidePathResolve(root,"仅生成😀")),*target2=KEEP(xidePathResolve(root,"导入并打开😀")),*target3=KEEP(xidePathResolve(root,"状态不匹配")),*image=KEEP(xidePathResolve(root,"native-import-review.png")),*imageLarge=KEEP(xidePathResolve(root,"native-import-large-font.png")),*imageOpened=KEEP(xidePathResolve(root,"native-import-restored.png"));
 CHECK(settings && sessions && source && project && xlPath && cPath && otherXl && missing && outside && target && target2 && target3 && image && imageLarge && imageOpened);
 CHECK(writeText(xlPath,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    return nativeAnswer()-42\nend\n") && writeText(cPath,"int native_answer(void) { return 42; }\n") && writeText(otherXl,"func main(...) as int\n    return 0\nend\n") && writeText(outside,"int outside(void) { return 1; }\n"));
 CHECK(writeText(project,"{\"Version\":1,\"Project\":{\"Description\":\"旧混合项目😀\",\"AppPath\":\"bin\"},\"Build\":{\"File\":\"out.bin\",\"Unknown\":{\"keep\":1}},\"File\":[{\"Path\":\"main.xl\",\"Open\":true},{\"Path\":\"alternate.xl\",\"Open\":false},{\"Path\":\"native.c\",\"Open\":true},{\"Path\":\"missing.txt\",\"Open\":true}],\"Resource\":[{\"Path\":\"missing.ico\",\"Open\":false}],\"Library\":[\"console\"],\"Status\":{\"ProjExpand\":true,\"FileExpand\":false,\"LibsExpand\":false,\"ResourceExpand\":false},\"BootCode\":\"\",\"Extra\":\"保留\"}") && writeText(source,"{\"Version\":1,\"Name\":\"旧工作空间😀\",\"NewFileIndex\":87,\"Status\":{\"IsExpand\":false},\"Projects\":[{\"Path\":\"old.xlp\",\"Open\":true},{\"Path\":\"missing.xlp\",\"Open\":false}],\"Files\":[{\"Path\":\"native.c\",\"Open\":true}],\"Extra\":\"工作空间原字段\"}"));
 oldBytes=(char*)xrtFileReadAll(source,&oldLength); oldProjectBytes=(char*)xrtFileReadAll(project,&oldProjectLength); CHECK(oldBytes && oldProjectBytes);
 app.hidden=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.sessionRoot=sessions; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==74 && xideActionNamed("project.importLegacy")->id==37 && xideActionQuery(&app,XIDE_ACTION_IMPORT_LEGACY).enabled && xideEditorOpen(&app,outside,&error)); xide_document* previous=app.activeDocument;
 CHECK(menuNew(&app) && xideImportViewIsOpen(&app) && xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && !xideTemplateViewOpen(&app) && !xideReferencesViewOpen(&app) && !xideConfigurationViewOpen(&app) && !xideFontViewOpen(&app) && !xideRecoveryViewOpen(&app) && !xideFileChangesViewOpen(&app));
 CHECK(fields(&app,source,target) && !xidePathExists(target) && !xuiWidgetGetEnabled(xideImportViewControl(&app,XIDE_IMPORT_APPLY)) && !xuiCheckBoxGetChecked(xideImportViewControl(&app,XIDE_IMPORT_REVIEWED)) && xuiComboBoxGetSelected(xideImportViewControl(&app,XIDE_IMPORT_STARTUP))==0);
 xui_widget preview=xideImportViewControl(&app,XIDE_IMPORT_PREVIEW); CHECK(xuiTextEditIsReadonly(preview) && xuiTextEditGetWordWrap(preview) && strstr(xuiTextEditGetText(preview),"kind-required") && capture(&app,image));
 CHECK(choose(&app,XIDE_IMPORT_SELECTED_PROJECT,1) && !xuiWidgetGetEnabled(xideImportViewControl(&app,XIDE_IMPORT_ENTRY)) && !choose(&app,XIDE_IMPORT_STARTUP,2));
 CHECK(choose(&app,XIDE_IMPORT_SELECTED_PROJECT,0) && choices(&app) && xuiWidgetGetEnabled(xideImportViewControl(&app,XIDE_IMPORT_APPLY)) && choose(&app,XIDE_IMPORT_PREVIEW_KIND,2) && strstr(xuiTextEditGetText(preview),"native.c"));
 char* previewCopy=KEEP(copy(xuiTextEditGetText(preview))); CHECK(previewCopy && xuiSetFocusWidget(app.context,preview)==XUI_OK && xuiInputText(app.context,'X')==XUI_OK && settle(&app) && !strcmp(previewCopy,xuiTextEditGetText(preview)));
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(previous->editor),0,"// 导入后修改\n")==XUI_OK && !xideImportViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(target) && click(&app,XIDE_IMPORT_CANCEL));
 CHECK(xideImportViewOpen(&app) && fields(&app,source,target) && choices(&app) && !xideImportViewApply(&app,&error) && previous->open && previous->dirty && !xidePathExists(target));
 CHECK(click(&app,XIDE_IMPORT_OPEN_AFTER) && !xuiCheckBoxGetChecked(xideImportViewControl(&app,XIDE_IMPORT_OPEN_AFTER)) && !xuiWidgetGetEnabled(xideImportViewControl(&app,XIDE_IMPORT_RESTORE_STATE)));
 CHECK(xideSessionSave(&app,0,&error)); char* previousSession=KEEP(xidePathResolve(xideSessionDirectory(&app),"session.json")); sessionBytes=(char*)xrtFileReadAll(previousSession,&sessionLength); CHECK(sessionBytes);
 CHECK(click(&app,XIDE_IMPORT_APPLY) && !xideImportViewIsOpen(&app) && xidePathExists(target) && previous->open && previous->dirty && !app.workspaceModel.source && opened(&app)==1);
 size_t afterLength=0; char* after=(char*)xrtFileReadAll(previousSession,&afterLength); CHECK(after && afterLength==sessionLength && !memcmp(after,sessionBytes,sessionLength)); xrtFree(after);
 CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(previous->editor))==XUI_OK && !previous->dirty && xideImportViewOpen(&app) && fields(&app,source,target2) && choices(&app));
 /* Session publication failure happens before bundle publication or editor close. */
 CHECK(xideSessionSave(&app,0,&error)); lock=lockFile(previousSession); CHECK(lock!=INVALID_HANDLE_VALUE && !xideImportViewApply(&app,&error) && !xidePathExists(target2) && previous->open && opened(&app)==1); CloseHandle(lock); lock=INVALID_HANDLE_VALUE;
 CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460) && xuiTextEditGetFont(preview)==app.uiFont && xuiInputGetFont(xideImportViewControl(&app,XIDE_IMPORT_ENTRY))==app.uiFont && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideImportViewWindow(&app)),xideImportViewControl(&app,XIDE_IMPORT_APPLY))==XUI_OK && capture(&app,imageLarge));
 CHECK(click(&app,XIDE_IMPORT_APPLY) && !xideImportViewIsOpen(&app) && xidePathExists(target2) && !xideDocumentsFind(&app.documents,outside) && opened(&app)==2 && app.workspaceModel.projectCount==2 && capture(&app,imageOpened));
 xide_document *xl=xideDocumentsFind(&app.documents,xlPath),*c=xideDocumentsFind(&app.documents,cPath); CHECK(xl && c && !strcmp(xl->language,"xlang") && !strcmp(c->language,"c") && !xl->dirty && !c->dirty && syntaxToken(xl->editor,"func main",XUI_CODE_TOKEN_KEYWORD) && syntaxToken(c->editor,"native_answer",XUI_CODE_TOKEN_IDENTIFIER));
 char *nativeWorkspace=KEEP(copy(app.workspaceModel.manifest)),*projectId=KEEP(copy(app.workspaceModel.projects[0].id)),*xlSession=KEEP(copy(xl->sessionId)),*cSession=KEEP(copy(c->sessionId)),*hintsPath=KEEP(xidePathResolve(target2,"session-hints.json")),*nativeSession=KEEP(xidePathResolve(xideSessionDirectory(&app),"session.json")); CHECK(nativeWorkspace && projectId && xlSession && cSession && hintsPath && nativeSession);
 CHECK(expanded(&app,"workspace","","",0) && expanded(&app,"project",projectId,"",1) && expanded(&app,"group",projectId,"group-0",0) && expanded(&app,"group",projectId,"group-1",0) && expanded(&app,"group",projectId,"libraries",0) && expanded(&app,"group",projectId,"group-3",0));
 saved=xrtJsonParseFile(nativeSession); hints=xrtJsonParseFile(hintsPath); CHECK(saved && hints && get(saved,"legacyImportHints") && xrtValueEqual(hints,get(saved,"legacyImportHints")) && xrtValueCount(get(saved,"openDocuments"))==3); xrtValueRelease(saved); saved=NULL;
 xide_legacy_session_result result={0}; CHECK(!xideSessionApplyLegacyHints(&app,hints,&result,&error) && error.code==XIDE_MODEL_CONFLICT && opened(&app)==2 && !result.applied);
 if(f->compiler) { xide_task_info info={0}; xide_task* t=xideTasksBuild(app.tasks,&app.workspaceModel.projects[0],NULL,f->compiler,1,&error); CHECK(t && taskWait(&app,t,&info)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"import UI compiler: %s\n",info.error); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); }
 CHECK(writeText(cPath,"int native_answer(void) { return 42; }\n// 外部更新\n")); uint64_t deadline=GetTickCount64()+5000; do { CHECK(settle(&app)); if(strstr(xuiCodeEditGetText(c->editor),"外部更新")) break; Sleep(20); } while(GetTickCount64()<deadline); CHECK(strstr(xuiCodeEditGetText(c->editor),"外部更新") && xideSessionSave(&app,0,&error));
 CHECK(writeText(missing,"恢复出现的文件\n"));
 xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.highDpi=1; app.settingsPath=settings; app.sessionRoot=sessions; app.workspacePath=nativeWorkspace; CHECK(xideAppCreate(&app)==XUI_OK && settle(&app) && opened(&app)==3);
 xl=xideDocumentsFind(&app.documents,xlPath); c=xideDocumentsFind(&app.documents,cPath); CHECK(xl && c && xideDocumentsFind(&app.documents,missing) && !strcmp(xl->sessionId,xlSession) && !strcmp(c->sessionId,cSession) && !strcmp(c->language,"c") && !strcmp(xl->language,"xlang") && expanded(&app,"workspace","","",0) && expanded(&app,"group",projectId,"group-1",0));
 CHECK(xideSessionSave(&app,0,&error)); saved=xrtJsonParseFile(nativeSession); CHECK(saved && xrtValueEqual(hints,get(saved,"legacyImportHints")) && xrtValueCount(get(saved,"openDocuments"))==3); xrtValueRelease(saved); saved=NULL;
 /* A generated-only scope has no native state. Tampered hints must have zero effects. */
 char* generatedWorkspace=KEEP(xidePathResolve(target,"workspace.xlw")); CHECK(generatedWorkspace && xideWorkspaceOpen(&app,generatedWorkspace,0,&error)); xvalue* reading=xideReadingCapture(&app,NULL); CHECK(reading && opened(&app)==0);
 char* generatedHints=KEEP(xidePathResolve(target,"session-hints.json")); bad=xrtJsonParseFile(generatedHints); CHECK(bad && put(xrtValueArrayGet(get(xrtValueArrayGet(get(bad,"projects"),0),"files"),0),"id","tampered") && !xideSessionApplyLegacyHints(&app,bad,&result,&error) && opened(&app)==0 && !result.applied); xvalue* currentReading=xideReadingCapture(&app,NULL); CHECK(currentReading && xrtValueEqual(reading,currentReading)); xrtValueRelease(reading); xrtValueRelease(currentReading); xrtValueRelease(bad); bad=NULL;
 CHECK(xideImportViewOpen(&app) && fields(&app,project,target3) && choose(&app,XIDE_IMPORT_CANDIDATES,0) && choose(&app,XIDE_IMPORT_KIND,1) && reviewed(&app));
 CHECK(xuiEditSetText(xideImportViewControl(&app,XIDE_IMPORT_ENTRY),otherXl)==XUI_OK && !xideImportViewApply(&app,&error) && !xidePathExists(target3) && click(&app,XIDE_IMPORT_SET_ENTRY) && !xuiCheckBoxGetChecked(xideImportViewControl(&app,XIDE_IMPORT_REVIEWED)) && reviewed(&app));
 CHECK(xuiEditSetText(xideImportViewControl(&app,XIDE_IMPORT_DESTINATION),target)==XUI_OK && settle(&app) && !xuiWidgetGetEnabled(xideImportViewControl(&app,XIDE_IMPORT_APPLY)) && click(&app,XIDE_IMPORT_CANCEL) && !xidePathExists(target3));
 after=(char*)xrtFileReadAll(source,&afterLength); CHECK(after && afterLength==oldLength && !memcmp(after,oldBytes,oldLength)); xrtFree(after); after=(char*)xrtFileReadAll(project,&afterLength); CHECK(after && afterLength==oldProjectLength && !memcmp(after,oldProjectBytes,oldProjectLength)); xrtFree(after);
 puts("import_view_test passed: native menu/buttons, explicit analysis and entry/kind/startup review, unsupported references, read-only wrapped preview, stale documents/dirty switch/session lock guards, generate-only isolation, deduplicated C/xlang syntax and missing-file session recovery, tree state/raw extensions/restart, file watch, 125 percent and 40 DIP two-axis popup, three fresh native captures.");
 if(f->compiler) puts("import_view_test real DEMO6 passed: imported mixed C/xlang project built and ran.");
cleanup:
 if(lock!=INVALID_HANDLE_VALUE) { CloseHandle(lock); } xrtValueRelease(hints); xrtValueRelease(bad); xrtValueRelease(saved); xrtFree(oldBytes); xrtFree(oldProjectBytes); xrtFree(sessionBytes); if(began) xgeEnd(); xideAppDestroy(&app); for(size_t i=0;i<ownedCount;++i) free(owned[i]); xgeQuit(); return XGE_OK;
#undef KEEP
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang legacy import verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{ if((argc!=2 && argc!=3) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangLegacyImportVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
