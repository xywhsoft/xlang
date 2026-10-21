#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "services/file_rebind.h"
#include "services/project_templates.h"
#include "services/file_sync.h"
#include "services/session.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/references_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"file_rebind_test:%d: %s; %s\n",__LINE__,#v,error.message); f->failed=1; goto cleanup; } } while(0)
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* s) { return xrtValueObjectGet(v,sv(s)); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static xide_file* file(xide_project* p,const char* path) { for(size_t i=0;i<p->files.count;++i) if(xidePathEqual(p->files.items[i].path,path)) return &p->files.items[i]; return NULL; }
static int equalBytes(const char* a,const char* b)
{ size_t na=0,nb=0; unsigned char *ra=xrtFileReadAllLimit(a,16u*1024u*1024u,&na),*rb=xrtFileReadAllLimit(b,16u*1024u*1024u,&nb); int okay=ra && rb && na==nb && !memcmp(ra,rb,na); xrtFree(ra); xrtFree(rb); return okay; }
static int settle(xide_app* a)
{ if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK; }
static int idle(xide_app* a)
{ uint64_t end=GetTickCount64()+10000; do { if(!settle(a)) return 0; if(xideFileSyncSettled(a)) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int taskWait(xide_app* a,xide_task* t,xide_task_info* info,int phase)
{ uint64_t end=GetTickCount64()+30000; do { if(!settle(a)) return 0; xideTaskInfo(t,info); if(phase>=0 ? (int)info->phase==phase : info->state>=XIDE_TASK_SUCCEEDED) return 1; if(phase>=0 && info->state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int capture(xide_app* a,const char* path)
{ if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK && !a->frameError; }
static int verify(void* user)
{
 fixture* f=user; if(f->started) return XGE_OK; f->started=1;
 xide_app app={0}; xide_model_error error={0}; xide_project round={0}; xide_template_plan* template=NULL; xide_file_rebind* plan=NULL; HANDLE lock=INVALID_HANDLE_VALUE; int began=0;
 char *root=NULL,*settings=NULL,*sessions=NULL,*workspace=NULL,*first=NULL,*second=NULL,*entry=NULL,*native=NULL,*alias=NULL,*newEntry=NULL,*newC=NULL,*blocked=NULL,*wrong=NULL,*occupied=NULL,*resource=NULL,*newResource=NULL,*manifestCopy=NULL,*imageXl=NULL,*imageC=NULL,*fake=NULL,*sourceId=NULL,*sharedId=NULL,*resourceId=NULL,*sessionId=NULL;
 xvalue* before=NULL;
 char* unique=xideCreateId("rebind"); root=unique ? xidePathResolve(f->directory,unique) : NULL; free(unique); CHECK(root && xrtDirCreateAll(root));
 settings=xidePathResolve(root,"settings.json"); sessions=xidePathResolve(root,"session"); workspace=xidePathResolve(root,"中文工作空间.xlw");
 newEntry=xidePathResolve(root,"另存入口😀.xl"); newC=xidePathResolve(root,"另存C源码.c"); alias=xidePathResolve(root,"共享硬链接.c"); blocked=xidePathResolve(root,"锁定目标.c"); wrong=xidePathResolve(root,"错误种类.txt"); occupied=xidePathResolve(root,"已引用.c"); resource=xidePathResolve(root,"资源.json"); newResource=xidePathResolve(root,"新资源.xlon"); manifestCopy=xidePathResolve(root,"清单副本.xlp"); imageXl=xidePathResolve(root,"native-rebind-xlang.png"); imageC=xidePathResolve(root,"native-rebind-c.png");
 CHECK(settings && sessions && workspace && newEntry && newC && alias && blocked && wrong && occupied && resource && newResource && manifestCopy && imageXl && imageC);
 app.hidden=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.sessionRoot=sessions;
 CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"另存为验证",&error));
 template=xideTemplatePlanPrepare(root,"原项目😀","mixed-c",&error); CHECK(template && xideTemplatePlanCommit(&app.workspaceModel,template,1,&first,&error)); xideTemplatePlanDestroy(template); template=NULL;
 template=xideTemplatePlanPrepare(root,"共享项目","mixed-c",&error); CHECK(template && xideTemplatePlanCommit(&app.workspaceModel,template,0,&second,&error)); xideTemplatePlanDestroy(template); template=NULL;
 xide_project* p=xideWorkspaceFindProject(&app.workspaceModel,first); xide_project* q=xideWorkspaceFindProject(&app.workspaceModel,second); CHECK(p && q);
 /* Load a real disk snapshot containing an unknown root field. */
 xvalue* seed=xrtJsonParseFile(p->manifest); CHECK(seed && put(seed,"futureRoot","保留根字段")); size_t bytes=0; char* serialized=xrtJsonStringify(seed,1,&bytes); CHECK(serialized && xrtFileWriteAtomic(p->manifest,(xbytesview){(const unsigned char*)serialized,bytes})); xrtFree(serialized);
 xide_project snapshot={0}; CHECK(xideProjectDecode(&snapshot,p->manifest,seed,&error)); xrtValueRelease(seed); snapshot.id=p->id; p->id=NULL; snapshot.referenceSource=p->referenceSource; p->referenceSource=NULL; xideProjectClear(p); *p=snapshot;
 entry=copy(p->entry); native=copy(p->cSources.items[0]); CHECK(entry && native);
 wchar_t oldW[32768],aliasW[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,native,-1,oldW,32768) && MultiByteToWideChar(CP_UTF8,0,alias,-1,aliasW,32768) && CreateHardLinkW(aliasW,oldW,NULL));
 char* oldEntryId=copy(file(q,q->entry)->id); char* oldCId=copy(file(q,q->cSources.items[0])->id); CHECK(oldEntryId && oldCId && xideProjectSetEntry(q,entry,&error) && xideProjectRemoveFile(q,oldEntryId,&error) && xideProjectRemoveFile(q,oldCId,&error) && xideProjectAddFile(q,alias,"c",&error)); free(oldEntryId); free(oldCId);
 CHECK(xideWorkspaceAddFile(&app.workspaceModel,alias,&error) && put(file(p,native)->source,"future","保留😀") && put(file(q,alias)->source,"future","共享字段"));
 sourceId=copy(file(p,native)->id); sharedId=copy(file(q,alias)->id); CHECK(sourceId && sharedId);
 CHECK(writeText(resource,"{\"format\":\"xlang-windows-resource-1\",\"version\":{\"fileVersion\":\"2.3.4.5\",\"productVersion\":\"2.3.4.5\"}}") && xideProjectAddFile(p,resource,"resource-config",&error)); resourceId=copy(file(p,resource)->id); CHECK(resourceId);
 CHECK(writeText(entry,"// FAKE_SLOW\nextern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    return 0\nend\n") && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,first) && xideWorkspaceSaveAll(&app,workspace,&error) && idle(&app));
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* xl=app.activeDocument; uint64_t id=xl->id; xui_widget editor=xl->editor; int window=xl->window; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)); sessionId=copy(xl->sessionId); CHECK(sessionId);
 char* entryId=copy(file(p,entry)->id); CHECK(entryId && xideWorkspaceViewSelectFile(&app,first,entryId)); int selected=xuiTreeViewGetSelected(app.workspace); free(entryId);
 CHECK(xideFileRebindPrepare(&app.workspaceModel,native,newC,&plan,&error) && plan && xideFileRebindProjectCount(plan)==2 && !xidePathExists(newC) && xidePathEqual(p->cSources.items[0],native) && xidePathEqual(q->cSources.items[0],alias)); xideFileRebindDestroy(plan); plan=NULL;
 /* An already-running build of the old project cannot run after a path switch. */
 wchar_t executable[32768]; CHECK(GetModuleFileNameW(NULL,executable,32768)); wchar_t* slash=wcsrchr(executable,L'\\'); CHECK(slash); wcscpy(slash+1,L"task_test.exe"); int n=WideCharToMultiByte(CP_UTF8,0,executable,-1,NULL,0,NULL,NULL); fake=n ? malloc((size_t)n) : NULL; CHECK(fake && WideCharToMultiByte(CP_UTF8,0,executable,-1,fake,n,NULL,NULL));
 xide_task* stale=xideTasksBuild(app.tasks,p,NULL,fake,1,&error); xide_task_info info={0}; CHECK(stale && taskWait(&app,stale,&info,XIDE_TASK_BUILD));
 CHECK(xideDocumentSave(xl,newEntry,0,&error) && xl->id==id && xl->editor==editor && xl->window==window && !strcmp(xl->sessionId,sessionId) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor))==version && !xl->dirty && !strcmp(xl->language,"xlang"));
 CHECK(xidePathEqual(p->entry,newEntry) && xidePathEqual(q->entry,newEntry) && p->dirty && q->dirty && !strcmp(app.workspaceModel.startupProject,first) && xuiTreeViewGetSelected(app.workspace)==selected && xidePathExists(entry) && equalBytes(entry,newEntry) && !app.fileRebind && !app.fileRebindReading);
 CHECK(taskWait(&app,stale,&info,-1) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran && capture(&app,imageXl));
 CHECK(xideEditorOpen(&app,native,&error) && settle(&app)); xide_document* c=app.activeDocument; xui_code_document core=xuiCodeEditGetDocument(c->editor); CHECK(xuiCodeDocumentInsert(core,0,"// 本地😀\n")==XUI_OK && c->dirty);
 before=xideProjectExport(p,&error); CHECK(before && writeText(blocked,"int locked = 8;\n") && MultiByteToWideChar(CP_UTF8,0,blocked,-1,oldW,32768)); lock=CreateFileW(oldW,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(lock!=INVALID_HANDLE_VALUE);
 CHECK(!xideDocumentSave(c,blocked,XIDE_DOCUMENT_OVERWRITE_TARGET,&error) && error.code==XIDE_MODEL_IO && c->dirty && xidePathEqual(c->path,native) && !app.fileRebind && !app.fileRebindReading);
 xvalue* after=xideProjectExport(p,&error); CHECK(after && xrtValueEqual(before,after)); xrtValueRelease(after); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; xrtValueRelease(before); before=NULL;
 CHECK(!xideDocumentSave(c,wrong,0,&error) && error.code==XIDE_MODEL_INVALID && !xidePathExists(wrong) && !app.fileRebind);
 CHECK(writeText(occupied,"int occupied = 9;\n") && xideProjectAddFile(p,occupied,"c",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);
 CHECK(!xideDocumentSave(c,occupied,XIDE_DOCUMENT_OVERWRITE_TARGET,&error) && error.code==XIDE_MODEL_CONFLICT && !app.fileRebind);
 CHECK(xideEditorOpen(&app,blocked,&error) && !xideDocumentSave(c,blocked,XIDE_DOCUMENT_OVERWRITE_TARGET,&error) && error.code==XIDE_MODEL_CONFLICT && !app.fileRebind);
 before=xideProjectConfigurationsJson(p); xvalue* analysis=xrtValueObject(),*forced=xrtValueArray();
 CHECK(before && analysis && forced && xrtValueArrayAppendNew(forced,xrtValueString(sv(native))) && xrtValueObjectSetNew(analysis,sv("forcedIncludes"),forced) && xrtValueObjectSetNew(xrtValueArrayGet(before,0),sv("cAnalysis"),analysis) && xideProjectApplyConfigurations(p,before,p->activeConfiguration,&error)); xrtValueRelease(before); before=NULL;
 CHECK(xideWorkspaceViewSelectProject(&app,first) && xideReferencesViewOpen(&app)); CHECK(!xideDocumentSave(c,newC,0,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(newC) && !app.fileRebind); CHECK(xideDialogSetOpen(xideReferencesViewWindow(&app),0)==XUI_OK);
 CHECK(xideEditorOpen(&app,p->manifest,&error)); xide_document* md=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(md->editor),0," ")==XUI_OK && !xideDocumentSave(c,newC,0,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(newC)); CHECK(xideDocumentReload(md,1,&error));
 CHECK(!xideDocumentSave(md,manifestCopy,0,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(manifestCopy));
 CHECK(xideEditorShow(&app,c)==XUI_OK); uint32_t cVersion=xuiCodeDocumentGetVersion(core); int a=0,b=0; CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),core,2,6)==XUI_OK && xideDocumentSave(c,newC,0,&error));
 CHECK(p->configurations[0].cAnalysis.forcedIncludes.count==1 && xidePathEqual(p->configurations[0].cAnalysis.forcedIncludes.items[0],newC));
 CHECK(xidePathEqual(p->cSources.items[0],newC) && xidePathEqual(q->cSources.items[0],newC) && xidePathEqual(app.workspaceModel.files.items[0].path,newC) && app.workspaceModel.dirty && !strcmp(file(p,newC)->id,sourceId) && !strcmp(file(q,newC)->id,sharedId) && get(file(p,newC)->source,"future") && get(p->source,"futureRoot") && xuiCodeDocumentGetVersion(core)==cVersion && xuiCodeSelectionGetRange(xuiCodeEditGetSelection(c->editor),&a,&b)==XUI_OK && a==2 && b==6 && capture(&app,imageC));
 CHECK(xuiCodeDocumentUndo(core)==XUI_OK && c->dirty && xuiCodeDocumentRedo(core)==XUI_OK && !c->dirty && xidePathExists(native) && xidePathEqual(native,alias));
 CHECK(xideEditorOpen(&app,resource,&error) && xideDocumentSave(app.activeDocument,newResource,0,&error) && xidePathEqual(p->resources.items[0],newResource) && !strcmp(file(p,newResource)->id,resourceId));
 /* Header references follow a compatible path without entering cSources. */
 char* folder=xidePathDirectory(native); char* header=folder ? xidePathResolve(folder,"native.h") : NULL; char* newHeader=xidePathResolve(root,"新头文件.h"); free(folder); CHECK(header && newHeader && file(p,header)); char* headerId=copy(file(p,header)->id); CHECK(headerId && xideEditorOpen(&app,header,&error) && xideDocumentSave(app.activeDocument,newHeader,0,&error) && !strcmp(app.activeDocument->language,"c") && !strcmp(file(p,newHeader)->id,headerId) && p->cSources.count==2 && equalBytes(header,newHeader)); free(headerId); free(header); free(newHeader);
 /* An independent document may change extension and syntax without a model edit. */
 char* independent=xidePathResolve(root,"独立变语法.xl"); CHECK(independent && xideEditorOpen(&app,blocked,&error) && xideDocumentSave(app.activeDocument,independent,0,&error) && !strcmp(app.activeDocument->language,"xlang") && equalBytes(blocked,independent) && !app.fileRebind); free(independent);
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error) && xideProjectLoad(&round,p->manifest,&error) && xidePathEqual(round.entry,newEntry) && xidePathEqual(round.cSources.items[0],newC) && xidePathEqual(round.resources.items[0],newResource) && !strcmp(file(&round,newC)->id,sourceId) && get(file(&round,newC)->source,"future") && get(round.source,"futureRoot")); xideProjectClear(&round);
 CHECK(idle(&app) && writeText(newC,"int native_answer(void) { return 42; } // 外部😀\n")); uint64_t until=GetTickCount64()+10000; do { CHECK(settle(&app)); if(strstr(xuiCodeEditGetText(c->editor),"外部😀")) break; Sleep(5); } while(GetTickCount64()<until); CHECK(strstr(xuiCodeEditGetText(c->editor),"外部😀") && !c->dirty);
 CHECK(xideSessionSave(&app,0,&error)); char* session=xidePathResolve(xideSessionDirectory(&app),"session.json"); CHECK(session); xvalue* saved=xrtJsonParseFile(session); free(session); CHECK(saved); int found=0; for(size_t i=0;i<xrtValueCount(get(saved,"openDocuments"));++i) { xvalue* item=xrtValueArrayGet(get(saved,"openDocuments"),i); xstrview path={0}; if(xrtValueGetString(get(item,"path"),&path)) { char* absolute=xidePathResolve(root,path.Data); if(absolute && xidePathEqual(absolute,newC)) found=1; free(absolute); } } xrtValueRelease(saved); CHECK(found);
 /* Clean Save As onto a hard-link spelling is a real path change, not a no-op. */
 CHECK(xideDocumentSave(c,newC,0,&error) && !app.fileRebind && !app.fileRebindReading); char* sameAlias=xidePathResolve(root,"同物理新拼写.c"); CHECK(sameAlias && MultiByteToWideChar(CP_UTF8,0,newC,-1,oldW,32768) && MultiByteToWideChar(CP_UTF8,0,sameAlias,-1,aliasW,32768) && CreateHardLinkW(aliasW,oldW,NULL)); CHECK(xideDocumentSave(c,sameAlias,0,&error) && !strcmp(c->path,sameAlias) && xidePathEqual(p->cSources.items[0],sameAlias) && xidePathEqual(q->cSources.items[0],sameAlias) && !xidePathEqual(newC,sameAlias) && equalBytes(newC,sameAlias)); free(sameAlias);
 CHECK(xideWorkspaceSaveAll(&app,NULL,&error));
 if(f->compiler) { xide_task* task=xideTasksBuild(app.tasks,p,NULL,f->compiler,1,&error); CHECK(task && taskWait(&app,task,&info,-1) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); puts("file_rebind_test real DEMO6 passed: relocated entry, C source and resource configuration built and ran."); }
 CHECK(!app.frameError && !app.fileRebind && !app.fileRebindReading);
 puts("file_rebind_test passed: synchronous preallocated reference transaction, shared entry/C/header/hardlinks/workspace files/resources, stable IDs/unknown fields and startup/tree, atomic failure and complete callback cleanup, kind/duplicate/open/manifest/draft protection, no-op and clean physical alias Save As, document/undo/selection/session, actual worker resubscription and stale task invalidation, two native screenshots.");
cleanup:
 xgeQuit();
 if(lock!=INVALID_HANDLE_VALUE) { CloseHandle(lock); } xideFileRebindDestroy(plan); xideTemplatePlanDestroy(template); xrtValueRelease(before); xideProjectClear(&round); if(began) xgeEnd(); xideAppDestroy(&app);
 free(root); free(settings); free(sessions); free(workspace); free(first); free(second); free(entry); free(native); free(alias); free(newEntry); free(newC); free(blocked); free(wrong); free(occupied); free(resource); free(newResource); free(manifestCopy); free(imageXl); free(imageC); free(fake); free(sourceId); free(sharedId); free(resourceId); free(sessionId); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang Save As reference verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{ if((argc!=2 && argc!=3) || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argc==3 ? argv[2] : NULL}; wchar_t name[100]; swprintf(name,100,L"XlangRebindVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code; }
