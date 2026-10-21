#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/rename_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "services/project_migration.h"
#include "services/session.h"
#include "ui/recovery_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Observation barriers wrap the actual production UI transaction. No fault
   injection or barrier is compiled into either delivered application. */
static const char *fixtureRoot,*scenario,*operation;
static xide_app* liveApp;
static int saveCalls,prepareCalls,commitCalls,failed;
static HDESK fixtureDesktop;
static void barrier(const char* phase);
static int has(const char* token) { return strstr(scenario,token)!=NULL; }
static int observePrepare(xide_app* app,const xide_project_migration* plan,xide_model_error* e)
{ ++prepareCalls; int okay=xideSessionPrepareProjectMigration(app,plan,e); if(okay && has("prepared") && (!has("chain") || prepareCalls==2)) barrier("prepared"); return okay; }
static int observeCommit(xide_project_migration* plan,xide_model_error* e)
{ ++commitCalls; int okay=xideProjectMigrationCommit(plan,e); if(okay && has("physical") && (!has("chain") || commitCalls==2)) barrier("physical"); return okay; }
static int observeSave(xide_app* app,int clean,xide_model_error* e)
{ ++saveCalls; int okay=xideSessionSave(app,clean,e); if(okay && has("after") && saveCalls==2 && !has("chain")) barrier("after"); return okay; }
static void observeRelocate(xide_app* app)
{ xideSessionRelocate(app); if(has("after") && !has("chain")) barrier("after"); }
#define xideSessionPrepareProjectMigration observePrepare
#define xideProjectMigrationCommit observeCommit
#define xideSessionSave observeSave
#define xideSessionRelocate observeRelocate
#ifndef XIDE_TEST_MIGRATION_VIEW
#define XIDE_TEST_MIGRATION_VIEW "ui/rename_view.c"
#endif
#include XIDE_TEST_MIGRATION_VIEW
#undef xideSessionPrepareProjectMigration
#undef xideProjectMigrationCommit
#undef xideSessionSave
#undef xideSessionRelocate

static const char *xlBody="// 未保存 xlang 😀\nfunc main(...) as int\n    return 0\nend\n",*cBody="// 未保存 C 😀\nint native_answer(void) { return 42; }\n",*outsideBody="// 外部 C 草稿 😀\nint outside = 7;\n",*untitledBody="int untitled_draft = 29; // 😀\n",*laterBody="// 后续 C 草稿 😀\nint native_answer(void) { return 43; }\n";
static xstrview fixtureSv(const char* s) { return (xstrview){s,strlen(s)}; }
static int putText(xvalue* o,const char* k,const char* s) { return xrtValueObjectSetNew(o,fixtureSv(k),xrtValueString(fixtureSv(s ? s : ""))); }
static int putInt(xvalue* o,const char* k,int n) { return xrtValueObjectSetNew(o,fixtureSv(k),xrtValueInt(n)); }
static int writeText(const char* path,const char* s) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int writeValue(const char* name,const xvalue* value)
{ char* path=xidePathResolve(fixtureRoot,name); size_t n=0; char* s=xrtJsonStringify(value,true,&n); int okay=path && s && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)s,n}); free(path); xrtFree(s); return okay; }
static int bodyPresent(const char* text)
{ for(size_t i=0;i<liveApp->documents.count;++i) { xide_document* d=liveApp->documents.items[i]; if(d->open && !strcmp(xuiCodeEditGetText(d->editor),text)) return 1; } return 0; }
static xvalue* record(const char* phase)
{
 xvalue* o=xrtValueObject(); int okay=o && putText(o,"phase",phase) && putInt(o,"pid",(int)GetCurrentProcessId()) && putText(o,"workspace",liveApp->workspaceModel.manifest) && putText(o,"projectOwner",liveApp->projectSessionPath) && putText(o,"scope",xideSessionDirectory(liveApp)) && putText(o,"issue",xideSessionIssue(liveApp)) && putInt(o,"xlangBody",bodyPresent(xlBody)) && putInt(o,"cBody",bodyPresent(cBody)) && putInt(o,"laterBody",bodyPresent(laterBody)) && putInt(o,"outsideBody",bodyPresent(outsideBody)) && putInt(o,"untitledBody",bodyPresent(untitledBody)) && putInt(o,"workspaceDirty",liveApp->workspaceModel.dirty);
 xide_project* p=liveApp->workspaceModel.projectCount ? &liveApp->workspaceModel.projects[0] : NULL;
 okay=okay && putText(o,"manifest",p ? p->manifest : "") && putText(o,"entry",p ? p->entry : "") && putText(o,"projectId",p ? p->id : "") && putText(o,"description",p ? p->description : "") && putInt(o,"property",p && !strcmp(p->description,"未保存项目属性 😀")) && putInt(o,"projectDirty",p && p->dirty) && putText(o,"activeId",liveApp->activeDocument ? liveApp->activeDocument->sessionId : "");
 int cLanguage=0,xlLanguage=0,selection=0,override=0; const char* cId="";
 for(size_t i=0;i<liveApp->documents.count;++i) { xide_document* d=liveApp->documents.items[i]; if(!d->open) continue; const char* body=xuiCodeEditGetText(d->editor);
  if(!strcmp(body,cBody) || !strcmp(body,laterBody)) { int a=0,b=0; cLanguage=!strcmp(d->language,"c"); override=d->languageOverride; cId=d->sessionId; xuiCodeSelectionGetRange(xuiCodeEditGetSelection(d->editor),&a,&b); selection=a==2 && b==6; }
  if(!strcmp(body,xlBody)) xlLanguage=!strcmp(d->language,"xlang");
 }
 okay=okay && putInt(o,"cLanguage",cLanguage) && putInt(o,"xlangLanguage",xlLanguage) && putInt(o,"cOverride",override) && putInt(o,"selection",selection) && putText(o,"cId",cId);
 if(!okay) { xrtValueRelease(o); return NULL; } return o;
}
static void barrier(const char* phase)
{ xvalue* value=record(phase); if(!value || !writeValue("barrier.json",value)) ExitProcess(81); xrtValueRelease(value); for(;;) Sleep(25); }
static int capture(const char* leaf)
{ char* path=xidePathResolve(fixtureRoot,leaf); xui_rect_i_t r={0,0,liveApp->viewportWidth,liveApp->viewportHeight}; int okay=path && xuiLayout(liveApp->context)==XUI_OK && xuiUpdate(liveApp->context,0)==XUI_OK && liveApp->proxy.surfaceClear(&liveApp->proxy,liveApp->target,0xffeeeeeeu)==XUI_OK && xuiRender(liveApp->context,liveApp->target,&r,1)==XUI_OK && xideAppCapture(liveApp,path)==XGE_OK; free(path); return okay; }
static char* migrationMarkerPath(void)
{
 char* index=xideSessionGlobalPath(liveApp,XIDE_GLOBAL_INDEX),*global=index ? xidePathDirectory(index) : NULL,*directory=global ? xidePathResolve(global,"migrations") : NULL,*pattern=directory ? xidePathResolveLexical(directory,"*") : NULL;
 wchar_t* wide=pattern ? xidePathNativeWide(pattern) : NULL; WIN32_FIND_DATAW data={0}; HANDLE find=wide ? FindFirstFileW(wide,&data) : INVALID_HANDLE_VALUE; char* marker=NULL;
 if(find!=INVALID_HANDLE_VALUE) { do { if(wcslen(data.cFileName)!=16) continue; char leaf[17]; for(int i=0;i<16;++i) leaf[i]=(char)data.cFileName[i]; leaf[16]=0; char* child=xidePathResolve(directory,leaf); marker=child ? xidePathResolve(child,"migration-journal.json") : NULL; free(child); if(marker && xidePathExists(marker)) break; free(marker); marker=NULL; } while(FindNextFileW(find,&data)); FindClose(find); }
 free(index); free(global); free(directory); free(pattern); free(wide); return marker;
}
static HANDLE lockMigrationMarker(void)
{
 char* marker=migrationMarkerPath(); wchar_t* name=marker ? xidePathNativeWide(marker) : NULL;
 HANDLE lock=name ? CreateFileW(name,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
 free(marker); free(name); return lock;
}
static int storeFiles(void)
{
 char* marker=migrationMarkerPath(),*directory=marker ? xidePathDirectory(marker) : NULL,*pattern=directory ? xidePathResolveLexical(directory,"recovery/*") : NULL;
 wchar_t* wide=pattern ? xidePathNativeWide(pattern) : NULL; WIN32_FIND_DATAW data={0}; HANDLE find=wide ? FindFirstFileW(wide,&data) : INVALID_HANDLE_VALUE; int count=0;
 if(find!=INVALID_HANDLE_VALUE) { do { if(wcscmp(data.cFileName,L".") && wcscmp(data.cFileName,L"..")) ++count; } while(FindNextFileW(find,&data)); if(GetLastError()!=ERROR_NO_MORE_FILES) count=-1; FindClose(find); } else count=-1;
 free(marker); free(directory); free(pattern); free(wide); return count;
}
static int fillStore(void)
{
 char* marker=migrationMarkerPath(),*directory=marker ? xidePathDirectory(marker) : NULL,*path=directory ? xidePathResolveLexical(directory,"recovery/unknown-retained.bin") : NULL;
 wchar_t* wide=path ? xidePathNativeWide(path) : NULL; HANDLE file=wide ? CreateFileW(wide,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
 LARGE_INTEGER length; length.QuadPart=512u*1024u*1024u;
 int okay=file!=INVALID_HANDLE_VALUE && SetFilePointerEx(file,length,NULL,FILE_BEGIN) && SetEndOfFile(file);
 if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
 free(marker); free(directory); free(path); free(wide); return okay;
}
#define VERIFY(v) do { if(!(v)) { fprintf(stderr,"migration_checkpoint_test:%d: %s; %s\n",__LINE__,#v,error.message); fflush(stderr); ExitProcess(81); } } while(0)
static int run(void* user)
{
 (void)user; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; char* owned[24]={0}; size_t count=0; int began=0;
#define OWN(v) (owned[count++]=(v))
 char *directory=OWN(xidePathResolve(fixtureRoot,"Project原😀")),*targetDirectory=OWN(xidePathResolveLexical(fixtureRoot,has("case-only") ? "PROJECT原😀" : "Moved项目😀")),*manifest=OWN(xidePathResolve(directory,"app.xlp")),*xl=OWN(xidePathResolve(directory,"main.xl")),*native=OWN(xidePathResolve(directory,"native.c")),*outside=OWN(xidePathResolve(fixtureRoot,"outside.c")),*workspace=OWN(xidePathResolve(has("external") ? fixtureRoot : directory,"work.xlw")),*targetManifest=OWN(xidePathResolveLexical(fixtureRoot,"renamed项目😀.xlp")),*newManifest=OWN(has("manifest") ? xidePathResolveLexical(NULL,targetManifest) : xidePathResolveLexical(targetDirectory,"app.xlp")),*newWorkspace=OWN(has("external") || has("manifest") ? xidePathResolveLexical(NULL,workspace) : xidePathResolveLexical(targetDirectory,"work.xlw")),*settings=OWN(xidePathResolve(fixtureRoot,"settings.json")),*sessions=OWN(xidePathResolve(fixtureRoot,"sessions")),*local=OWN(xidePathResolve(fixtureRoot,"local"));
 VERIFY(directory && targetDirectory && manifest && xl && native && outside && workspace && targetManifest && newManifest && newWorkspace && settings && sessions && local);
 int inspecting=strncmp(operation,"inspect",7)==0;
 if(!inspecting) {
  VERIFY(xrtDirCreateAll(directory) && writeText(xl,"func main(...) as int\n    return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && writeText(outside,"int outside = 7;\n") && xideProjectCreate(&project,manifest,"Migration project",xl,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error));
  if(!has("solo")) { VERIFY(xideWorkspaceCreate(&app.workspaceModel,"Migration",&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"project",&error)); if(!has("unnamed")) { VERIFY(xideWorkspaceSave(&app.workspaceModel,workspace,&error)); xideWorkspaceClear(&app.workspaceModel); } }
 }
 if(has("default")) { wchar_t* wide=xidePathNativeWide(local); VERIFY(wide && SetEnvironmentVariableW(L"LOCALAPPDATA",wide)); free(wide); }
 app.hidden=!has("default"); app.settingsPath=settings; app.sessionRoot=has("default") ? NULL : sessions; app.noSession=has("no-session");
 if(!has("unnamed") && strcmp(operation,"inspect-recent")) { int migrated=!strcmp(operation,"inspect-new") || !strncmp(operation,"inspect-store-",14); const char* path=migrated ? (has("solo") ? newManifest : newWorkspace) : (has("solo") ? manifest : workspace); if(has("solo")) app.projectPath=path; else app.workspacePath=path; }
 VERIFY(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; app.hidden=1; liveApp=&app;
 if(inspecting) {
  if(!strcmp(operation,"inspect-edit")) { for(size_t i=0;i<app.documents.count;++i) { xide_document* d=app.documents.items[i]; if(d->open && !strcmp(xuiCodeEditGetText(d->editor),cBody)) VERIFY(xuiCodeDocumentInsert(xuiCodeEditGetDocument(d->editor),0,"// 恢复后编辑\n")==XUI_OK && xuiCodeDocumentUndo(xuiCodeEditGetDocument(d->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(d->editor),cBody)); } }
  if(!strcmp(operation,"inspect-maintain")) { xide_recovery_stats stats={0}; VERIFY(!xideSessionMaintain(&app,&stats,&error)); }
  int before=-1,after=-1;
  if(!strncmp(operation,"inspect-store-",14)) {
   before=storeFiles(); VERIFY(before>0);
   if(!strcmp(operation,"inspect-store-maintain")) { xide_recovery_stats stats={0}; VERIFY(xideSessionMaintain(&app,&stats,&error)); }
   else if(!strcmp(operation,"inspect-store-auto")) { uint64_t end=GetTickCount64()+15000; while(storeFiles()>0 && GetTickCount64()<end) { xideSessionPoll(&app); Sleep(2); } }
   else if(!strcmp(operation,"inspect-store-view")) {
    VERIFY(xideRecoveryViewOpen(&app)); uint64_t end=GetTickCount64()+15000;
    while(xideRecoveryViewBusy(&app) && GetTickCount64()<end) { xideRecoveryViewPoll(&app); Sleep(2); }
    VERIFY(!xideRecoveryViewBusy(&app));
    xui_widget clean=xideRecoveryViewControl(&app,XIDE_RECOVERY_CLEAN);
    VERIFY(clean && xuiDispatchPendingEvents(app.context)==XUI_OK && xuiLayout(app.context)==XUI_OK);
    VERIFY(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideRecoveryViewWindow(&app)),clean)==XUI_OK && xuiLayout(app.context)==XUI_OK);
    xui_rect_t rect=xuiWidgetGetWorldRect(clean);
    int px=(int)(rect.fX+rect.fW/2),py=(int)(rect.fY+rect.fH/2);
    VERIFY(xuiInputPointerMove(app.context,px,py,0)==XUI_OK && xuiInputPointerDown(app.context,px,py,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app.context,px,py,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
    end=GetTickCount64()+15000; while(xideRecoveryViewBusy(&app) && GetTickCount64()<end) { xideRecoveryViewPoll(&app); Sleep(2); }
    VERIFY(!xideRecoveryViewBusy(&app) && strstr(xideRecoveryViewMessage(&app),"退休迁移正文"));
   }
   after=storeFiles();
  }
  xvalue* value=record("inspected"); char name[64]; snprintf(name,sizeof(name),"%s.json",operation); VERIFY(value && putInt(value,"storeBefore",before) && putInt(value,"storeAfter",after) && writeValue(name,value)); xrtValueRelease(value); VERIFY(capture("native-migration-recovery.png")); ExitProcess(0);
 }
 VERIFY(app.workspaceModel.projectCount==1 && xideWorkspaceViewSelectProject(&app,app.workspaceModel.projects[0].id) && xideEditorOpen(&app,xl,&error)); xide_document* x=app.activeDocument;
 VERIFY(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument;
 VERIFY(xideEditorOpen(&app,outside,&error)); xide_document* ext=app.activeDocument;
 VERIFY(xuiCodeDocumentReplace(xuiCodeEditGetDocument(x->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(x->editor)),xlBody)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(c->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),cBody)==XUI_OK && xuiCodeDocumentReplace(xuiCodeEditGetDocument(ext->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(ext->editor)),outsideBody)==XUI_OK && xideDocumentSetLanguage(c,"c",&error));
 xide_document* u=xideDocumentNew(&app.documents,"c",&error); VERIFY(u && xideEditorShow(&app,u)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(u->editor),0,untitledBody)==XUI_OK && xideEditorShow(&app,c)==XUI_OK && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),2,6)==XUI_OK);
 xide_project* p=&app.workspaceModel.projects[0]; VERIFY(xideProjectSetMetadata(p,p->name,"未保存项目属性 😀",p->kind,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideSessionSave(&app,0,&error));
 xvalue* request=record("request"); VERIFY(request && putText(request,"oldWorkspace",workspace) && putText(request,"newWorkspace",newWorkspace) && putText(request,"oldManifest",manifest) && putText(request,"newManifest",newManifest) && putText(request,"source",has("manifest") ? manifest : directory) && putText(request,"target",has("manifest") ? targetManifest : targetDirectory) && writeValue("request.json",request)); xrtValueRelease(request);
 VERIFY(xideRenameViewOpenProject(&app,has("manifest") ? XIDE_MIGRATE_MANIFEST : XIDE_MIGRATE_DIRECTORY,&error) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),has("manifest") ? targetManifest : targetDirectory)==XUI_OK && xideRenameViewRefresh(&app) && capture("native-migration-dialog.png"));
 int applied=xideRenameViewApply(&app,&error); VERIFY(applied);
 int storeBefore=-1,storeRepeated=-1,storeChanged=-1,storeRepeatedChanged=-1;
 if(has("store") || has("quota")) {
  storeBefore=storeFiles(); VERIFY(storeBefore>0);
  for(int i=0;i<20;++i) VERIFY(xideSessionSave(&app,0,&error));
  storeRepeated=storeFiles(); VERIFY(storeRepeated==storeBefore);
  if(has("quota")) { VERIFY(fillStore()); int full=storeFiles(); VERIFY(full==storeBefore+1 && xideSessionSave(&app,0,&error) && storeFiles()==full); }
 }
 if(has("chain")) { char* second=OWN(xidePathResolveLexical(fixtureRoot,"Second项目😀.xlp")); VERIFY(second && xideRenameViewOpenProject(&app,XIDE_MIGRATE_MANIFEST,&error) && xuiEditSetText(xideRenameViewControl(&app,XIDE_RENAME_NAME),second)==XUI_OK && xideRenameViewRefresh(&app)); VERIFY(xideRenameViewApply(&app,&error)); }
 if(has("later")) {
  VERIFY(xuiCodeDocumentReplace(xuiCodeEditGetDocument(c->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),laterBody)==XUI_OK);
  HANDLE lock=has("newer") ? lockMigrationMarker() : INVALID_HANDLE_VALUE;
  if(has("newer")) VERIFY(lock!=INVALID_HANDLE_VALUE);
  int saved=xideSessionSave(&app,0,&error); if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
  VERIFY(has("newer") || has("quota") ? !saved : saved);
  if(has("store") || has("quota")) {
   storeChanged=storeFiles(); VERIFY(storeChanged==storeBefore+1);
   if(has("store")) { for(int i=0;i<20;++i) VERIFY(xideSessionSave(&app,0,&error)); storeRepeatedChanged=storeFiles(); VERIFY(storeRepeatedChanged==storeChanged); }
  }
 }
 if(has("saved")) VERIFY(xideWorkspaceSaveAll(&app,has("solo") ? OWN(xidePathResolve(fixtureRoot,"saved.xlw")) : NULL,&error) && xideSessionSave(&app,0,&error));
 xvalue* result=record("completed"); VERIFY(result && putInt(result,"storeBefore",storeBefore) && putInt(result,"storeRepeated",storeRepeated) && putInt(result,"storeChanged",storeChanged) && putInt(result,"storeRepeatedChanged",storeRepeatedChanged) && writeValue("result.json",result)); xrtValueRelease(result); ExitProcess(0);
 if(began) { xgeEnd(); } xideAppDestroy(&app); xideProjectClear(&project); for(size_t i=0;i<count;++i) free(owned[i]); return XGE_OK;
#undef OWN
}
static DWORD WINAPI worker(void* user)
{ (void)user; if(!SetThreadDesktop(fixtureDesktop)) return 1; xge_desc_t desc={0}; desc.iWidth=1200; desc.iHeight=900; desc.sTitle="xlang migration checkpoint fixture"; desc.iFlags=XGE_INIT_WINDOW; desc.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(run,NULL); xgeUnit(); return failed || result!=XGE_OK; }
int main(int argc,char** argv)
{
 if(argc!=4) { return 2; } fixtureRoot=argv[1]; scenario=argv[2]; operation=argv[3];
 wchar_t name[80]; swprintf(name,80,L"XlangMigrationCheckpoint-%lu",GetCurrentProcessId()); fixtureDesktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!fixtureDesktop) return 3;
 HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(80); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(fixtureDesktop); return (int)code;
}
