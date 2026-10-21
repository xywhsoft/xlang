#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/project_templates.h"
#include "services/project_migration.h"
#include "services/file_sync.h"
#include "services/session.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"project_migration_view_test:%d: %s; model=%s; popup=%s\n",__LINE__,#v,error.message,xideRenameViewMessage(&app)); f->failed=1; goto cleanup; } } while(0)
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static char* keep(char** list,size_t* count,char* value) { if(value) list[(*count)++]=value; return value; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int writeJson(const char* p,const xvalue* v) { size_t n=0; char* s=xrtJsonStringify(v,1,&n); int okay=s && xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,n}); xrtFree(s); return okay; }
static xide_file* file(xide_files* files,const char* path) { for(size_t i=0;i<files->count;++i) if(xidePathEqual(files->items[i].path,path)) return &files->items[i]; return NULL; }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK; }
static int idle(xide_app* a) { uint64_t end=GetTickCount64()+10000; do { if(!settle(a)) return 0; if(xideFileSyncSettled(a)) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int taskWait(xide_app* a,xide_task* t,xide_task_info* info,int phase) { uint64_t end=GetTickCount64()+30000; do { if(!settle(a)) return 0; xideTaskInfo(t,info); if(phase>=0 ? (int)info->phase==phase : info->state>=XIDE_TASK_SUCCEEDED) return 1; if(phase>=0 && info->state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int waitBuildGate(xide_app* a,xide_task* task)
{
 uint64_t end=GetTickCount64()+10000,cursor=0,dropped=0; char log[4096]={0}; size_t at=0; xide_task_info info={0};
 do { if(!settle(a)) return 0; xideTaskInfo(task,&info);
  if(at<sizeof(log)-1) { at+=xideTaskReadLog(task,&cursor,log+at,sizeof(log)-at-1,&dropped); }
  log[at]=0;
  if(dropped || info.state>=XIDE_TASK_SUCCEEDED) return 0;
  if(strstr(log,"BUILD_GATE_WAITING")) return info.phase==XIDE_TASK_BUILD && !info.built && !info.ran;
  Sleep(5);
 } while(GetTickCount64()<end); return 0;
}
static size_t opened(xide_app* a) { size_t n=0; for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open) ++n; return n; }
static int capture(xide_app* a,const char* path) { if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id) {
 xui_widget w=xideRenameViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideRenameViewWindow(a)); if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
 xui_rect_t r=xuiWidgetGetWorldRect(w),v=xuiWidgetGetWorldRect(scroll); float right=r.fX+r.fW,bottom=r.fY+r.fH;
 if(r.fX<v.fX) r.fX=v.fX;
 if(r.fY<v.fY) r.fY=v.fY;
 if(right>v.fX+v.fW) right=v.fX+v.fW;
 if(bottom>v.fY+v.fH) bottom=v.fY+v.fH;
 r.fW=right-r.fX; r.fH=bottom-r.fY; return r.fW>0 && r.fH>0 && pointer(a,r);
}
static int destination(xide_app* a,const char* value) { return xuiEditSetText(xideRenameViewControl(a,XIDE_RENAME_NAME),value)==XUI_OK && xideRenameViewRefresh(a); }
static int menu(xide_app* a,int action) { int index=-1; for(int i=0;i<xuiMenuGetItemCount(a->projectMenu);++i) if(xuiMenuGetItem(a->projectMenu,i)->iValue==action) index=i; if(index<0 || xuiMenuOpenAt(a->projectMenu,a->root,80,60)!=XUI_OK || !settle(a)) return 0; xui_rect_t r=xuiMenuGetItemRect(a->projectMenu,index),w=xuiWidgetGetWorldRect(a->projectMenu); r.fX+=w.fX; r.fY+=w.fY; return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK && settle(a) && xuiMenuGetHoverIndex(a->projectMenu)==index && pointer(a,r); }
static HANDLE lockDirectory(const char* p) { wchar_t w[32768]; return MultiByteToWideChar(CP_UTF8,0,p,-1,w,32768) ? CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL) : INVALID_HANDLE_VALUE; }
static int resize(xide_app* a,int width,int height) { xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface next=NULL; if(a->proxy.surfaceCreate(&a->proxy,&next,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=next; a->viewportWidth=width; a->viewportHeight=height; return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a); }
static char* fakeCompiler(void) { wchar_t w[32768]; if(!GetModuleFileNameW(NULL,w,32768)) return NULL; wchar_t* slash=wcsrchr(w,L'\\'); if(!slash) return NULL; wcscpy(slash+1,L"task_test.exe"); int n=WideCharToMultiByte(CP_UTF8,0,w,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p && !WideCharToMultiByte(CP_UTF8,0,w,-1,p,n,NULL,NULL)) { free(p); p=NULL; } return p; }
typedef struct editing { uint64_t id; uint32_t version; xui_widget editor; int window,dirty,override,bom,eol,start,end; const void *raw,*language,*session; } editing;
static editing snapshot(xide_document* d) { editing s={d->id,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),d->editor,d->window,d->dirty,d->languageOverride,d->baseline.bom,d->baseline.eol,0,0,d->baseline.raw,d->language,d->sessionId}; xuiCodeSelectionGetRange(xuiCodeEditGetSelection(d->editor),&s.start,&s.end); return s; }
static int preserved(xide_document* d,const editing* s) { int start=0,end=0; return d->open && d->id==s->id && d->editor==s->editor && d->window==s->window && d->dirty==s->dirty && d->languageOverride==s->override && d->baseline.bom==s->bom && d->baseline.eol==s->eol && d->baseline.raw==s->raw && d->language==s->language && d->sessionId==s->session && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))==s->version && xuiCodeSelectionGetRange(xuiCodeEditGetSelection(d->editor),&start,&end)==XUI_OK && start==s->start && end==s->end; }
static int startDefaultSession(xide_app* a) { a->hidden=0; int okay=xideSessionStart(a); a->hidden=1; return okay; }
static int verify(void* user) {
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_template_plan* creation=NULL; xvalue* json=NULL; xide_text_file empty={0}; xide_document_relocation* relocation=NULL; HANDLE lock=INVALID_HANDLE_VALUE,buildGate=NULL; int began=0; char* owned[100]={0}; size_t count=0;
#define KEEP(v) keep(owned,&count,(v))
 char* unique=xideCreateId("migration-ui"); char* root=KEEP(unique ? xidePathResolve(f->directory,unique) : NULL); free(unique); CHECK(root && xrtDirCreateAll(root));
 char *settings=KEEP(xidePathResolve(root,"settings.json")),*sessions=KEEP(xidePathResolve(root,"sessions")),*target=KEEP(xidePathResolve(root,"Moved项目😀")),*outside=KEEP(xidePathResolve(root,"外部文档.xl")),*imageDirectory=KEEP(xidePathResolve(root,"native-project-directory-migration.png")),*imageManifest=KEEP(xidePathResolve(root,"native-project-manifest-migration.png")); CHECK(settings && sessions && target && outside && imageDirectory && imageManifest);
 app.hidden=1; app.maxFrames=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.sessionRoot=sessions; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideActionCount()==73 && xideActionNamed("project.moveManifest")->id==35 && xideActionNamed("project.moveDirectory")->id==36 && !xideActionQuery(&app,XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY).enabled && xideWorkspaceCreate(&app.workspaceModel,"原生迁移",&error));
 char *pId=NULL,*qId=NULL;
 creation=xideTemplatePlanPrepare(root,"原项目😀","mixed-c",&error); CHECK(creation && xideTemplatePlanCommit(&app.workspaceModel,creation,1,&pId,&error)); KEEP(pId); xideTemplatePlanDestroy(creation); creation=NULL;
 creation=xideTemplatePlanPrepare(root,"共享项目","mixed-c",&error); CHECK(creation && xideTemplatePlanCommit(&app.workspaceModel,creation,0,&qId,&error)); KEEP(qId); xideTemplatePlanDestroy(creation); creation=NULL;
 xide_project *p=xideWorkspaceFindProject(&app.workspaceModel,pId),*q=xideWorkspaceFindProject(&app.workspaceModel,qId); CHECK(p && q);
 char *directory=KEEP(xidePathDirectory(p->manifest)),*manifest=KEEP(copy(p->manifest)),*entry=KEEP(copy(p->entry)),*native=KEEP(copy(p->cSources.items[0])),*workspace=KEEP(xidePathResolve(directory,"工作空间.xlw")),*qDirectory=KEEP(xidePathDirectory(q->manifest)),*manifestTarget=KEEP(xidePathResolve(qDirectory,"移动后的项目清单😀.xlp")); CHECK(directory && manifest && entry && native && workspace && qDirectory && manifestTarget);
 CHECK(writeText(entry,"// FAKE_GATE\nextern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && writeText(outside,"func main(...) as int\n    return 0\nend\n"));
 char* qEntryId=KEEP(copy(file(&q->files,q->entry)->id)),*qCId=KEEP(copy(file(&q->files,q->cSources.items[0])->id)),*cId=KEEP(copy(file(&p->files,native)->id)); CHECK(qEntryId && qCId && cId);
 CHECK(xideProjectSetEntry(q,entry,&error) && xideProjectRemoveFile(q,qEntryId,&error) && xideProjectRemoveFile(q,qCId,&error) && xideProjectAddFile(q,native,"c",&error) && xideWorkspaceAddFile(&app.workspaceModel,manifest,&error) && put(p->source,"futureRoot","保留字段") && put(file(&p->files,native)->source,"futureFile","保留 C 身份")); p->dirty=1; CHECK(writeJson(p->manifest,p->source) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceSaveAll(&app,workspace,&error) && xideWorkspaceViewSelectFile(&app,pId,cId));
 q->available=0; CHECK(xideWorkspaceViewSelectProject(&app,qId) && !xideActionQuery(&app,XIDE_ACTION_MIGRATE_PROJECT_MANIFEST).enabled); q->available=1; CHECK(xideWorkspaceViewSelectFile(&app,pId,cId));
 CHECK(xideEditorOpen(&app,entry,&error)); xide_document* xl=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"// 未保存 xlang 😀\n")==XUI_OK && xuiCodeFoldStateFoldAll(xuiCodeEditGetFoldState(xl->editor))==XUI_OK);
 CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// 未保存 C 😀\n")==XUI_OK && xideDocumentSetLanguage(c,"c",&error) && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),2,6)==XUI_OK);
 CHECK(xideEditorOpen(&app,manifest,&error)); xide_document* md=app.activeDocument; CHECK(xideEditorOpen(&app,workspace,&error)); xide_document* wd=app.activeDocument;
 xide_document* untitled=xideDocumentNew(&app.documents,"c",&error); CHECK(untitled && xideEditorShow(&app,untitled)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(untitled->editor),0,"int unsaved = 1;\n")==XUI_OK && xideEditorOpen(&app,outside,&error)); xide_document* other=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(other->editor),0,"// 外部未保存\n")==XUI_OK && opened(&app)==6 && idle(&app));
 /* Frozen dialog protects every editor, including an unrelated active document. */
 CHECK(xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,target) && xuiCodeDocumentInsert(xuiCodeEditGetDocument(other->editor),0,"// 服务级修改\n")==XUI_OK && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xidePathExists(directory) && !xidePathExists(target) && click(&app,XIDE_RENAME_CANCEL));
 CHECK(xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,target) && put(p->source,"stale","模型改变") && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xrtValueObjectRemove(p->source,sv("stale")) && click(&app,XIDE_RENAME_CANCEL));
 CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(md->editor),0," ")==XUI_OK && xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,target) && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xuiCodeDocumentUndo(xuiCodeEditGetDocument(md->editor))==XUI_OK && !md->dirty);
 /* An already-open recovery at a future destination must not be merged. */
 char* future=KEEP(xidePathResolve(target,xidePathFilename(entry))); CHECK(future && xideTextFileDecode((const unsigned char*)"",0,&empty,&error)); xide_document* occupied=xideDocumentRecover(&app.documents,future,"目标恢复","xlang",&empty,"// 目标恢复正文\n",&error); CHECK(occupied && xideEditorShow(&app,occupied)==XUI_OK && xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,target) && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL) && xideEditorClose(&app,occupied,1,&error) && xideEditorShow(&app,other)==XUI_OK);
 CHECK(xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,target)); lock=lockDirectory(directory); CHECK(lock!=INVALID_HANDLE_VALUE && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_IO && !strcmp(xuiEditGetText(xideRenameViewControl(&app,XIDE_RENAME_NAME)),target) && xideRenameViewIsOpen(&app)); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; CHECK(click(&app,XIDE_RENAME_CANCEL));
 /* A same-byte replaced project manifest invalidates the original file identity. */
 CHECK(xideRenameViewOpenProject(&app,XIDE_MIGRATE_MANIFEST,&error) && destination(&app,manifestTarget) && writeJson(manifest,p->source) && !xideRenameViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && click(&app,XIDE_RENAME_CANCEL));
 CHECK(xideWorkspaceViewSelectFile(&app,pId,cId) && idle(&app));
 editing xs=snapshot(xl),cs=snapshot(c),os=snapshot(other),us=snapshot(untitled),ms=snapshot(md),ws=snapshot(wd); size_t slots=app.documents.count; char* oldScope=KEEP(copy(xideSessionDirectory(&app))); char* keyValue=xideWorkspaceTreeKey(&app,xuiTreeViewGetSelected(app.workspace)); char* treeKey=KEEP(keyValue ? copy(keyValue) : NULL); xrtFree(keyValue); CHECK(oldScope && treeKey);
 CHECK(menu(&app,XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY));
 CHECK(xideRenameViewIsOpen(&app));
 CHECK(destination(&app,target));
 CHECK(capture(&app,imageDirectory));
 CHECK(click(&app,XIDE_RENAME_APPLY));
 CHECK(!xideRenameViewIsOpen(&app));
 char *newEntry=KEEP(xidePathResolve(target,xidePathFilename(entry))),*newC=KEEP(xidePathResolve(target,xidePathFilename(native))),*newManifest=KEEP(xidePathResolve(target,xidePathFilename(manifest))),*newWorkspace=KEEP(xidePathResolve(target,xidePathFilename(workspace))); CHECK(newEntry && newC && newManifest && newWorkspace);
 p=xideWorkspaceFindProject(&app.workspaceModel,pId); q=xideWorkspaceFindProject(&app.workspaceModel,qId); CHECK(p && q);
 CHECK(!xidePathExists(directory) && xidePathExists(target) && !strcmp(p->entry,newEntry) && !strcmp(q->entry,newEntry) && !strcmp(p->cSources.items[0],newC) && !strcmp(q->cSources.items[0],newC) && !strcmp(app.workspaceModel.manifest,newWorkspace) && p->dirty && q->dirty && app.workspaceModel.dirty && !strcmp(app.workspaceModel.startupProject,pId) && !strcmp(file(&p->files,newC)->id,cId) && get(file(&p->files,newC)->source,"futureFile"));
 CHECK(!strcmp(xl->path,newEntry) && !strcmp(c->path,newC) && !strcmp(md->path,newManifest) && !strcmp(wd->path,newWorkspace) && !strcmp(other->path,outside) && !untitled->path && preserved(xl,&xs) && preserved(c,&cs) && preserved(md,&ms) && preserved(wd,&ws) && preserved(other,&os) && preserved(untitled,&us) && app.activeDocument==other && app.documents.count==slots && opened(&app)==6);
 char* afterKey=xideWorkspaceTreeKey(&app,xuiTreeViewGetSelected(app.workspace)); CHECK(afterKey && !strcmp(afterKey,treeKey)); xrtFree(afterKey);
 int visible=1; CHECK(xuiCodeFoldStateIsLineVisible(xuiCodeEditGetFoldState(xl->editor),4,&visible)==XUI_OK && !visible && strcmp(oldScope,xideSessionDirectory(&app)) && xidePathExists(oldScope));
 CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !c->dirty && xuiCodeDocumentRedo(xuiCodeEditGetDocument(c->editor))==XUI_OK && c->dirty && xideDocumentSave(c,NULL,0,&error) && xideDocumentSave(xl,NULL,0,&error) && xideWorkspaceSaveAll(&app,NULL,&error) && idle(&app));
 /* Live build from the old manifest is invalidated by a file-only move. */
 wchar_t gateName[128]; swprintf(gateName,128,L"XlangMigrationBuildGate-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 buildGate=CreateEventW(NULL,TRUE,FALSE,gateName); CHECK(buildGate && SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",gateName));
 char* fake=KEEP(fakeCompiler()); xide_task_info info={0}; xide_task* stale=fake ? xideTasksBuild(app.tasks,p,NULL,fake,1,&error) : NULL; CHECK(stale && taskWait(&app,stale,&info,XIDE_TASK_BUILD) && waitBuildGate(&app,stale));
 CHECK(xideRenameViewOpenProject(&app,XIDE_MIGRATE_MANIFEST,&error) && destination(&app,manifestTarget) && xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460) && xuiInputGetFont(xideRenameViewControl(&app,XIDE_RENAME_NAME))==app.uiFont && capture(&app,imageManifest));
 Sleep(1200); xideTaskInfo(stale,&info); CHECK(info.phase==XIDE_TASK_BUILD && info.state==XIDE_TASK_EXECUTING && !info.built && !info.ran && click(&app,XIDE_RENAME_APPLY));
 CHECK(!strcmp(p->manifest,manifestTarget) && !strcmp(md->path,manifestTarget) && !strcmp(md->title,xidePathFilename(manifestTarget)) && !strcmp(p->entry,newEntry) && !xidePathExists(newManifest) && xidePathExists(newEntry) && app.activeDocument==other);
 CHECK(taskWait(&app,stale,&info,-1)); printf("migration gated task: phase=%d state=%d invalidated=%d built=%d ran=%d durationMs=%llu\n",info.phase,info.state,info.invalidated,info.built,info.ran,(unsigned long long)info.durationMs);
 CHECK(info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran); CloseHandle(buildGate); buildGate=NULL; CHECK(SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",NULL));
 CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && resize(&app,1280,800) && xideWorkspaceSaveAll(&app,NULL,&error) && idle(&app));
 CHECK(writeText(newC,"int native_answer(void) { return 42; } // 外部迁移后😀\n")); uint64_t until=GetTickCount64()+10000; do { CHECK(settle(&app)); if(strstr(xuiCodeEditGetText(c->editor),"外部迁移后😀")) break; Sleep(5); } while(GetTickCount64()<until); CHECK(strstr(xuiCodeEditGetText(c->editor),"外部迁移后😀") && !c->dirty && !xideFileSyncCount(&app) && get(p->source,"futureRoot"));
 if(f->compiler) { const char* ids[]={pId,qId}; for(size_t i=0;i<2;++i) { xide_task* t=xideTasksBuild(app.tasks,xideWorkspaceFindProject(&app.workspaceModel,ids[i]),NULL,f->compiler,1,&error); CHECK(t && taskWait(&app,t,&info,-1)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"migration UI compiler: %s\n",info.error); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); } }
 CHECK(xideSessionSave(&app,0,&error)); char *otherSession=KEEP(copy(other->sessionId)),*untitledSession=KEEP(copy(untitled->sessionId)),*xlSession=KEEP(copy(xl->sessionId)); CHECK(otherSession && untitledSession && xlSession && !app.frameError);
 xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.maxFrames=1; app.highDpi=1; app.settingsPath=settings; app.sessionRoot=sessions; app.workspacePath=newWorkspace; CHECK(xideAppCreate(&app)==XUI_OK && idle(&app) && opened(&app)==6);
 xl=xideDocumentsFind(&app.documents,newEntry); c=xideDocumentsFind(&app.documents,newC); other=xideDocumentsFind(&app.documents,outside); CHECK(xl && c && other && !strcmp(xl->language,"xlang") && !strcmp(c->language,"c") && c->languageOverride && !strcmp(xl->sessionId,xlSession) && !strcmp(other->sessionId,otherSession) && other->dirty && strstr(xuiCodeEditGetText(other->editor),"服务级修改"));
 untitled=NULL; for(size_t i=0;i<app.documents.count;++i) if(app.documents.items[i]->open && !strcmp(app.documents.items[i]->sessionId,untitledSession)) untitled=app.documents.items[i]; CHECK(untitled && !untitled->path && untitled->dirty && !strcmp(untitled->language,"c"));
 xideAppDestroy(&app); memset(&app,0,sizeof(app));
 /* Production default stores sessions below the project. Use a process-only
    LOCALAPPDATA fixture, so no real recent/session index is touched. */
 char* local=KEEP(xidePathResolve(root,"isolated-localappdata")); wchar_t localWide[32768]; CHECK(local && MultiByteToWideChar(CP_UTF8,0,local,-1,localWide,32768) && SetEnvironmentVariableW(L"LOCALAPPDATA",localWide));
 app.hidden=1; app.maxFrames=1; app.highDpi=1; app.settingsPath=settings; CHECK(xideAppCreate(&app)==XUI_OK && xideWorkspaceCreate(&app.workspaceModel,"独立测试",&error)); char* singleId=NULL;
 creation=xideTemplatePlanPrepare(root,"独立默认会话","mixed-c",&error); CHECK(creation && xideTemplatePlanCommit(&app.workspaceModel,creation,1,&singleId,&error)); KEEP(singleId); xideTemplatePlanDestroy(creation); creation=NULL;
 p=xideWorkspaceFindProject(&app.workspaceModel,singleId); char* singleManifest=KEEP(copy(p->manifest)); CHECK(singleManifest && xideWorkspaceOpen(&app,singleManifest,1,&error) && startDefaultSession(&app));
 p=&app.workspaceModel.projects[0]; char* singleDirectory=KEEP(xidePathDirectory(p->manifest)),*singleTarget=KEEP(xidePathResolve(root,"独立会话迁移😀")),*defaultScope=KEEP(copy(xideSessionDirectory(&app))); CHECK(singleDirectory && singleTarget && defaultScope && strstr(defaultScope,"/.xlang-ide/workspaces/"));
 CHECK(xideEditorOpen(&app,p->entry,&error)); xl=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,"// 默认会话未保存\n")==XUI_OK && xideEditorOpen(&app,p->cSources.items[0],&error)); c=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,"// 默认 C 未保存\n")==XUI_OK && xideWorkspaceViewSelectProject(&app,p->id));
 char *defaultXlSession=KEEP(copy(xl->sessionId)),*defaultCSession=KEEP(copy(c->sessionId)); CHECK(defaultXlSession && defaultCSession && xideRenameViewOpenProject(&app,XIDE_MIGRATE_DIRECTORY,&error) && destination(&app,singleTarget) && click(&app,XIDE_RENAME_APPLY) && !xidePathExists(singleDirectory) && !strncmp(app.projectSessionPath,singleTarget,strlen(singleTarget)) && !strncmp(xideSessionDirectory(&app),singleTarget,strlen(singleTarget)) && !xidePathExists(defaultScope) && !strcmp(xl->language,"xlang") && !strcmp(c->language,"c") && xl->dirty && c->dirty);
 CHECK(xideProjectSave(p,NULL,&error) && xideSessionSave(&app,0,&error)); char *defaultManifest=KEEP(copy(p->manifest)),*defaultEntry=KEEP(copy(p->entry)),*defaultC=KEEP(copy(p->cSources.items[0])); CHECK(defaultManifest && defaultEntry && defaultC && !app.frameError);
 xideAppDestroy(&app); memset(&app,0,sizeof(app)); app.hidden=1; app.maxFrames=1; app.highDpi=1; app.settingsPath=settings; app.projectPath=defaultManifest; CHECK(xideAppCreate(&app)==XUI_OK && startDefaultSession(&app) && idle(&app) && opened(&app)==2);
 xl=xideDocumentsFind(&app.documents,defaultEntry); c=xideDocumentsFind(&app.documents,defaultC); CHECK(xl && c && xl->dirty && c->dirty && !strcmp(xl->sessionId,defaultXlSession) && !strcmp(c->sessionId,defaultCSession) && strstr(xuiCodeEditGetText(xl->editor),"默认会话未保存") && strstr(xuiCodeEditGetText(c->editor),"默认 C 未保存") && !app.frameError);
 puts("project_migration_view_test passed: real menu/buttons, stale whole-document/model and changed-byte-identity/dirty manifest/open target/source-lock guards; batch dirty C/xlang/clean manifest paths, undo/selection/folds/raw and syntax/session IDs, stable tree and other dirty/untitled editors; shared references, old task invalidation, new-path external watch, 125 percent /40 DIP two-axis popup, custom workspace and production-default project session relocation and actual restart recovery; two native screenshots.");
 if(f->compiler) puts("project_migration_view_test real DEMO6 passed: migrated and shared mixed C/xlang projects rebuilt and ran.");
cleanup:
 xgeQuit(); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock); xideTemplatePlanDestroy(creation); xideDocumentsDestroyRelocation(relocation); xideTextFileClear(&empty); xrtValueRelease(json); if(began) xgeEnd(); xideAppDestroy(&app); if(buildGate) CloseHandle(buildGate); SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",NULL); for(size_t i=0;i<count;++i) free(owned[i]); return XGE_OK;
#undef KEEP
}
static DWORD WINAPI worker(void* user) { fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang project migration verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if((argc!=2 && argc!=3) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangProjectMigrationVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
