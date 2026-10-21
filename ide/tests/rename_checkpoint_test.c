#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "services/resource_rebind.h"
#include "services/session.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile the actual rename view with test-only observation barriers. The
   delivered application contains neither these barriers nor failure switches. */
static const char *testRoot,*testCase;
static xide_app* testApp;
static int checkpointCalls;
static int prepareCalls,resourceCalls;
static void stopAt(const char* phase);
static HANDLE checkpointMarkerLock(xide_app* a)
{ char* path=xidePathResolve(xideSessionDirectory(a),"rename-journal.json"); wchar_t* wide=path ? xidePathNativeWide(path) : NULL; HANDLE h=wide ? CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE; free(wide); free(path); return h; }
static int observedSave(xide_app* a,int clean,xide_model_error* e)
{
 ++checkpointCalls;
 if(!strcmp(testCase,"post-conflict") && checkpointCalls==2) {
  char* p=xidePathResolve(xideSessionDirectory(a),"session.json");
  if(p) { xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)"{conflict",9}); free(p); }
 }
 HANDLE lock=!strcmp(testCase,"newer-pending") && checkpointCalls==2 ? checkpointMarkerLock(a) : INVALID_HANDLE_VALUE;
 int result=xideSessionSave(a,clean,e); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
 if(result && ((!strcmp(testCase,"before") && checkpointCalls==1) || (!strcmp(testCase,"after") && checkpointCalls==2))) stopAt(checkpointCalls==1 ? "before" : "after");
 return result;
}
static int observedResources(xide_resource_rebind* p,xide_model_error* e)
{
 ++resourceCalls;
 if(!strcmp(testCase,"physical") || !strcmp(testCase,"case-physical") || (!strcmp(testCase,"chain-physical") && resourceCalls==2)) stopAt(testCase);
 if(!strcmp(testCase,"pending") || !strcmp(testCase,"newer-pending")) {
  e->code=XIDE_MODEL_MEMORY; snprintf(e->message,sizeof(e->message),"文件已改名；测试公开资源补丁失败。"); return 0;
 }
 return xideResourceRebindApply(p,e);
}
static int observedPrepare(xide_app* a,const char* source,const char* target,const xide_resource_rebind* p,xide_model_error* e)
{
 ++prepareCalls; int result=xideSessionPrepareRename(a,source,target,p,e);
 if(result && ((!strcmp(testCase,"journal") && prepareCalls==1) || (!strcmp(testCase,"chain-before") && prepareCalls==2))) stopAt(testCase);
 return result;
}
static int observedFinish(xide_app* a,const xide_resource_rebind* p,xide_model_error* e)
{ HANDLE lock=!strcmp(testCase,"finish-failure-undo") ? checkpointMarkerLock(a) : INVALID_HANDLE_VALUE; int result=xideSessionFinishRename(a,p,e); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock); return result; }
#define xideSessionSave observedSave
#define xideResourceRebindApply observedResources
#define xideSessionPrepareRename observedPrepare
#define xideSessionFinishRename observedFinish
#ifndef XIDE_TEST_RENAME_VIEW
#define XIDE_TEST_RENAME_VIEW "ui/rename_view.c"
#endif
#include XIDE_TEST_RENAME_VIEW
#undef xideSessionSave
#undef xideResourceRebindApply
#undef xideSessionPrepareRename
#undef xideSessionFinishRename

static int checks,failed;
static HDESK testDesktop;
static const char* xlBefore="func main(...) as int\n    return 0\nend\n";
static const char* cBefore="int native_answer(void) { return 42; }\n";
static const char* xlDirty="// 未保存 xlang 😀\nfunc main(...) as int\n    return 0\nend\n";
static const char* cDirty="// 未保存 C 😀\nint native_answer(void) { return 42; }\n";
static const char* resourceBefore="{\"icon\":\"old.ico\",\"future\":\"disk\"}";
static const char* resourceDirty="{\"icon\":\"old.ico\",\"future\":\"未保存资源 😀\"}";
static const char* untitledDirty="int unsaved_untitled = 23; // 😀\n";
static const char* cLater="// 后续 C 草稿 😀\nint native_answer(void) { return 43; }\n";
static const char* resourceLater="{\"icon\":\"new😀.ico\",\"future\":\"后续资源草稿 😀\"}";
static xstrview checkpointSv(const char* s) { return (xstrview){s,strlen(s)}; }
static int checkpointText(xvalue* o,const char* k,const char* s) { return xrtValueObjectSetNew(o,checkpointSv(k),xrtValueString(checkpointSv(s ? s : ""))); }
static int checkpointNumber(xvalue* o,const char* k,int n) { return xrtValueObjectSetNew(o,checkpointSv(k),xrtValueInt(n)); }
static int writeText(const char* p,const char* body) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)body,strlen(body)}); }
static int writeRecord(const char* name,xvalue* o)
{
 char* p=xidePathResolve(testRoot,name); size_t length=0; char* text=xrtJsonStringify(o,true,&length); int okay=p && text && xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)text,length}); xrtFree(text); free(p); return okay;
}
static int bodyPresent(xide_app* a,const char* body)
{ for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open && !strcmp(xuiCodeEditGetText(a->documents.items[i]->editor),body)) return 1; return 0; }
static xvalue* record(const char* phase,int applied,int errorCode)
{
 xvalue* o=xrtValueObject(); char *old=xidePathResolve(testRoot,"old.ico"),*next=xidePathResolveLexical(testRoot,"new😀.ico");
 int okay=o && checkpointText(o,"phase",phase) && checkpointText(o,"case",testCase) && checkpointText(o,"sessionDirectory",xideSessionDirectory(testApp)) && checkpointNumber(o,"pid",(int)GetCurrentProcessId()) && checkpointNumber(o,"checkpointCalls",checkpointCalls) && checkpointNumber(o,"applied",applied) && checkpointNumber(o,"errorCode",errorCode) && checkpointNumber(o,"sourceExists",xidePathExists(old)) && checkpointNumber(o,"targetExists",xidePathExists(next)) && checkpointNumber(o,"xlangBody",bodyPresent(testApp,xlDirty)) && checkpointNumber(o,"cBody",bodyPresent(testApp,cDirty)) && checkpointNumber(o,"untitledBody",bodyPresent(testApp,untitledDirty)) && checkpointNumber(o,"resourceBefore",bodyPresent(testApp,resourceDirty)) && checkpointNumber(o,"pending",xideRenameViewHasPending(testApp)) && checkpointText(o,"status",xideRenameViewMessage(testApp));
 xide_document* active=testApp->activeDocument; okay=okay && checkpointNumber(o,"activeC",active && !strcmp(xuiCodeEditGetText(active->editor),cDirty));
 int patched=0;
 for(size_t i=0;i<testApp->documents.count;++i) { xide_document* d=testApp->documents.items[i]; if(d->open && strstr(xuiCodeEditGetText(d->editor),"new😀.ico")) patched=1; }
 okay=okay && checkpointNumber(o,"resourcePatched",patched);
 int rebound=0; for(size_t i=0;i<testApp->workspaceModel.files.count;++i) if(!strcmp(xidePathFilename(testApp->workspaceModel.files.items[i].path),"new😀.ico")) rebound=1;
 okay=okay && checkpointNumber(o,"workspaceRenamed",rebound) && checkpointNumber(o,"workspaceDirty",testApp->workspaceModel.dirty) && checkpointText(o,"sessionIssue",xideSessionIssue(testApp));
 int first=0,projectRebound=0; xide_project* project=testApp->workspaceModel.projectCount ? &testApp->workspaceModel.projects[0] : NULL;
 for(size_t i=0;i<testApp->workspaceModel.files.count;++i) first|=!strcmp(xidePathFilename(testApp->workspaceModel.files.items[i].path),"first.ico");
 for(size_t i=0;project && i<project->files.count;++i) projectRebound|=!strcmp(xidePathFilename(project->files.items[i].path),"new😀.ico");
 okay=okay && checkpointNumber(o,"workspaceFirst",first) && checkpointNumber(o,"projectRenamed",projectRebound) && checkpointNumber(o,"projectDirty",project && project->dirty) && checkpointNumber(o,"unsavedProperty",project && !strcmp(project->description,"未保存工程属性 😀")) && checkpointNumber(o,"resourceFirst",0) && checkpointNumber(o,"cLater",bodyPresent(testApp,cLater)) && checkpointNumber(o,"resourceLater",bodyPresent(testApp,resourceLater));
 for(size_t i=0;i<testApp->documents.count;++i) if(testApp->documents.items[i]->open && strstr(xuiCodeEditGetText(testApp->documents.items[i]->editor),"first.ico")) checkpointNumber(o,"resourceFirst",1);
 int upper=0,resourceUpper=0;
 for(size_t i=0;i<testApp->workspaceModel.files.count;++i) upper|=!strcmp(xidePathFilename(testApp->workspaceModel.files.items[i].path),"OLD.ICO");
 for(size_t i=0;i<testApp->documents.count;++i) if(testApp->documents.items[i]->open) resourceUpper|=strstr(xuiCodeEditGetText(testApp->documents.items[i]->editor),"OLD.ICO")!=NULL;
 okay=okay && checkpointNumber(o,"workspaceUpper",upper) && checkpointNumber(o,"resourceUpper",resourceUpper);
 free(old); free(next); if(!okay) { xrtValueRelease(o); return NULL; } return o;
}
static void stopAt(const char* phase)
{
 xvalue* o=record(phase,-1,0); int okay=o && writeRecord("barrier.json",o); xrtValueRelease(o);
 if(!okay) ExitProcess(79);
 /* The harness terminates this exact owned process after inspecting the durable
    checkpoint. No cleanup or clean-exit checkpoint runs after the barrier. */
 for(;;) Sleep(1000);
}
#define VERIFY(value) do { ++checks; if(!(value)) { fprintf(stderr,"rename_checkpoint_test:%d: %s; %s\n",__LINE__,#value,error.message); failed=1; goto done; } } while(0)
static int run(void* user)
{
 (void)user; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; int began=0; char* owned[16]={0}; size_t count=0;
#define OWN(value) (owned[count++]=(value))
 char *workspace=OWN(xidePathResolve(testRoot,"work.xlw")),*manifest=OWN(xidePathResolve(testRoot,"app.xlp")),*xl=OWN(xidePathResolve(testRoot,"main.xl")),*native=OWN(xidePathResolve(testRoot,"native.c")),*resource=OWN(xidePathResolve(testRoot,"app.res.json")),*asset=OWN(xidePathResolve(testRoot,"old.ico")),*settings=OWN(xidePathResolve(testRoot,"settings.json")),*sessions=OWN(xidePathResolve(testRoot,"sessions"));
 VERIFY(workspace && manifest && xl && native && resource && asset && settings && sessions);
 int inspecting=!strncmp(testCase,"inspect",7);
 if(!inspecting) {
  VERIFY(xrtDirCreateAll(testRoot) && writeText(xl,xlBefore) && writeText(native,cBefore) && writeText(resource,resourceBefore));
  unsigned char ico[70]={0}; ico[2]=1; ico[4]=1; ico[6]=1; ico[7]=1; ico[10]=1; ico[12]=32; ico[14]=48; ico[18]=22; ico[22]=40; ico[26]=1; ico[30]=2; ico[34]=1; ico[36]=32; ico[42]=4; ico[65]=0xff;
  VERIFY(xrtFileWriteAtomic(asset,(xbytesview){ico,sizeof(ico)}) && xideProjectCreate(&project,manifest,"Checkpoint C/xlang",xl,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectAddFile(&project,resource,"resource-config",&error) && xideProjectAddFile(&project,asset,NULL,&error) && xideProjectSave(&project,NULL,&error));
  VERIFY(xideWorkspaceCreate(&app.workspaceModel,"Checkpoint",&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"project",&error) && xideWorkspaceAddFile(&app.workspaceModel,asset,&error) && xideWorkspaceSave(&app.workspaceModel,workspace,&error));
 } else VERIFY(xideWorkspaceLoad(&app.workspaceModel,workspace,&error));
 app.hidden=1; app.settingsPath=settings; app.sessionRoot=sessions; app.noSession=!strcmp(testCase,"no-session");
 VERIFY(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; testApp=&app;
 if(inspecting) {
  if(!strcmp(testCase,"inspect-undo")) { xide_document* recovered=xideDocumentsFind(&app.documents,resource); VERIFY(recovered && xuiCodeDocumentUndo(xuiCodeEditGetDocument(recovered->editor))==XUI_OK && (!strcmp(xuiCodeEditGetText(recovered->editor),resourceDirty) || strstr(xuiCodeEditGetText(recovered->editor),"first.ico"))); }
  if(!strcmp(testCase,"inspect-maintain")) { xide_recovery_stats stats={0}; VERIFY(!xideSessionMaintain(&app,&stats,&error)); }
  char resultName[64]; snprintf(resultName,sizeof(resultName),"%s.json",!strcmp(testCase,"inspect") ? "inspection" : testCase);
  xvalue* value=record("inspected",0,error.code); VERIFY(value && writeRecord(resultName,value)); xrtValueRelease(value); ExitProcess(0);
 }
 VERIFY(xideEditorOpen(&app,xl,&error)); xide_document* x=app.activeDocument;
 VERIFY(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument;
 VERIFY(xideEditorOpen(&app,resource,&error)); xide_document* r=app.activeDocument;
 VERIFY(xideSessionSave(&app,0,&error));
 VERIFY(xuiCodeDocumentReplace(xuiCodeEditGetDocument(x->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(x->editor)),xlDirty)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(c->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),cDirty)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(r->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(r->editor)),resourceDirty)==XUI_OK);
 xide_document* u=xideDocumentNew(&app.documents,"c",&error); VERIFY(u && xideEditorShow(&app,u)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(u->editor),0,untitledDirty)==XUI_OK && xideEditorShow(&app,c)==XUI_OK);
 xide_project* live=&app.workspaceModel.projects[0]; VERIFY(xideProjectSetMetadata(live,live->name,"未保存工程属性 😀",live->kind,&error));
 int chain=!strncmp(testCase,"chain-",6);
 const xide_file* selected=NULL; for(size_t i=0;i<app.workspaceModel.files.count;++i) if(xidePathEqual(app.workspaceModel.files.items[i].path,asset)) selected=&app.workspaceModel.files.items[i];
 VERIFY(selected && xideWorkspaceViewSelectFile(&app,NULL,selected->id) && xideRenameViewOpenFile(&app,asset,&error) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),!strcmp(testCase,"noop") ? "old.ico" : !strcmp(testCase,"case-physical") ? "OLD.ICO" : chain ? "first.ico" : "new😀.ico")==XUI_OK && xideRenameViewRefresh(&app));
 if(!strcmp(testCase,"pre-conflict")) { char* p=OWN(xidePathResolve(xideSessionDirectory(&app),"session.json")); VERIFY(p && writeText(p,"{conflict")); }
 int applied=xideRenameViewApply(&app,&error);
 if(chain) { char* first=OWN(xidePathResolve(testRoot,"first.ico")); VERIFY(applied && first && xideWorkspaceViewSelectFile(&app,NULL,app.workspaceModel.files.items[0].id) && xideRenameViewOpenFile(&app,first,&error) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),"new😀.ico")==XUI_OK && xideRenameViewRefresh(&app)); applied=xideRenameViewApply(&app,&error); }
 if(!strcmp(testCase,"undo") || !strcmp(testCase,"newer-undo") || !strcmp(testCase,"finish-failure-undo")) { VERIFY(applied && xuiCodeDocumentUndo(xuiCodeEditGetDocument(r->editor))==XUI_OK); if(strcmp(testCase,"newer-undo")) VERIFY(xideSessionSave(&app,0,&error)); }
 if(!strcmp(testCase,"edit-after")) { VERIFY(applied && xuiCodeDocumentReplace(xuiCodeEditGetDocument(c->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),cLater)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(r->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(r->editor)),resourceLater)==XUI_OK && xideSessionSave(&app,0,&error)); }
 if(!strcmp(testCase,"newer-checkpoint") || !strcmp(testCase,"newer-undo")) {
  if(!strcmp(testCase,"newer-checkpoint")) VERIFY(applied && xuiCodeDocumentReplace(xuiCodeEditGetDocument(c->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),cLater)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(r->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(r->editor)),resourceLater)==XUI_OK);
  char* marker=OWN(xidePathResolve(xideSessionDirectory(&app),"rename-journal.json")); wchar_t* wide=xidePathNativeWide(marker);
  HANDLE lock=wide ? CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE; free(wide); VERIFY(lock!=INVALID_HANDLE_VALUE);
  int saved=xideSessionSave(&app,0,&error); CloseHandle(lock); VERIFY(!saved);
 }
 if(!strcmp(testCase,"saved-references") || !strcmp(testCase,"save-all")) { VERIFY(applied && xideWorkspaceSaveAll(&app,NULL,&error)); if(!strcmp(testCase,"save-all")) VERIFY(xideDocumentSave(r,NULL,0,&error)); VERIFY(xideSessionSave(&app,0,&error)); }
 xvalue* value=record("completed",applied,error.code); VERIFY(value && writeRecord("result.json",value)); xrtValueRelease(value);
 /* Preserve the operation's own checkpoint result; destruction would publish
    a clean-exit snapshot and hide the crash interval under examination. */
 ExitProcess(0);
done:
 if(began) { xgeEnd(); } xideAppDestroy(&app); xideProjectClear(&project); for(size_t i=0;i<count;++i) free(owned[i]); return XGE_OK;
#undef OWN
}
static DWORD WINAPI checkpointWorker(void* user)
{
 (void)user; if(!SetThreadDesktop(testDesktop)) return 1;
 xge_desc_t desc={0}; desc.iWidth=1200; desc.iHeight=900; desc.sTitle="xlang rename checkpoint fixture"; desc.iFlags=XGE_INIT_WINDOW; desc.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(run,NULL); xgeUnit(); return failed || result!=XGE_OK;
}
int main(int argc,char** argv)
{
 if(argc!=3) { return 2; } testRoot=argv[1]; testCase=argv[2];
 wchar_t desktopName[80]; swprintf(desktopName,80,L"XlangRenameCheckpoint-%lu",GetCurrentProcessId());
 testDesktop=CreateDesktopW(desktopName,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
 if(!testDesktop) return 3;
 HANDLE thread=CreateThread(NULL,0,checkpointWorker,NULL,0,NULL); DWORD code=1;
 if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(80); GetExitCodeThread(thread,&code); CloseHandle(thread); }
 CloseDesktop(testDesktop); return (int)code;
}
