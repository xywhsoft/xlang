#define _WIN32_WINNT 0x0601
#include "services/legacy_import.h"
#include "services/tasks.h"
#include "platform/path.h"
#include "platform/file_rename.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"legacy_import_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return xrtValueObjectGet(v,sv(k)); }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int string(const xvalue* v,const char* s) { xstrview text={0}; return xrtValueGetString(v,&text) && text.Size==strlen(s) && !memcmp(text.Data,s,text.Size); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int writeJson(const char* p,const xvalue* v) { size_t n=0; char* json=xrtJsonStringify(v,1,&n); int okay=json && xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)json,n}); xrtFree(json); return okay; }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w; }
static int attributes(const char* p,DWORD flags) { wchar_t* w=wide(p); int okay=w && SetFileAttributesW(w,flags); free(w); return okay; }
static int removeEmpty(const char* p) { wchar_t* w=wide(p); int okay=w && RemoveDirectoryW(w); free(w); return okay; }
static int deleteFile(const char* p) { wchar_t* w=wide(p); int okay=w && DeleteFileW(w); free(w); return okay; }
static HANDLE openWrite(const char* p) { wchar_t* w=wide(p); HANDLE h=w ? CreateFileW(w,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL) : INVALID_HANDLE_VALUE; free(w); return h; }
static int issueCount(const xvalue* report,const char* code)
{ int n=0; for(size_t i=0;i<xrtValueCount(get(report,"issues"));++i) if(string(get(xrtValueArrayGet(get(report,"issues"),i),"code"),code)) ++n; return n; }
static int addRef(xvalue* a,const char* path,int open)
{ xvalue* v=xrtValueObject(); int okay=v && put(v,"Path",path) && xrtValueObjectSetNew(v,sv("Open"),xrtValueBool(open!=0)) && put(v,"Future","保留未知😀"); if(okay) return xrtValueArrayAppendNew(a,v); xrtValueRelease(v); return 0; }
static xvalue* oldProject(int multiple)
{
 const char* raw="{\"Version\":1,\"Project\":{\"Description\":\"中文混编😀\",\"AppPath\":\"bin\",\"Icon\":\"旧 UI 图标\"},\"Status\":{\"ProjExpand\":true,\"FileExpand\":false},\"Build\":{\"File\":\"imported.exe\",\"Icon\":\"res/app.ico\",\"FileVersion1\":1,\"Charset\":\"UTF-8\",\"FutureBuild\":17},\"File\":[],\"Resource\":[],\"Library\":[\"console\",\"lib/legacy.dll\"],\"BootCode\":\"legacy boot\",\"FutureRoot\":{\"value\":\"保留😀\"}}";
 xvalue* v=xrtJsonParse(sv(raw));
 int okay=v && addRef(get(v,"File"),"src/main.xl",1) && addRef(get(v,"File"),"src/native.c",0) && addRef(get(v,"File"),"src/native.h",0) && addRef(get(v,"Resource"),"res/data.json",0) && addRef(get(v,"Resource"),"res/missing.ico",0);
 if(okay && multiple) okay=addRef(get(v,"File"),"src/alternate.xl",1);
 if(!okay) { xrtValueRelease(v); return NULL; } return v;
}
static int waitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info)
{ uint64_t end=GetTickCount64()+30000; do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int stageCount(const char* root)
{
 char* pattern=xidePathResolve(root,".xide-import-*"); wchar_t* w=pattern ? wide(pattern) : NULL; WIN32_FIND_DATAW data; HANDLE h=w ? FindFirstFileW(w,&data) : INVALID_HANDLE_VALUE; int count=0;
 if(h!=INVALID_HANDLE_VALUE) { do { ++count; } while(FindNextFileW(h,&data)); FindClose(h); } free(pattern); free(w); return count;
}
typedef struct race { const char *parent,*target,*source; HANDLE ready; int created,locked; } race;
static DWORD WINAPI occupy(void* user)
{
 race* r=user; wchar_t* parent=wide(r->parent); HANDLE change=parent ? FindFirstChangeNotificationW(parent,FALSE,FILE_NOTIFY_CHANGE_DIR_NAME) : INVALID_HANDLE_VALUE; free(parent); SetEvent(r->ready);
 if(change!=INVALID_HANDLE_VALUE && WaitForSingleObject(change,15000)==WAIT_OBJECT_0) {
  HANDLE h=openWrite(r->source); r->locked=h==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION; if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
  wchar_t* target=wide(r->target); r->created=target && CreateDirectoryW(target,NULL); free(target);
 }
 if(change!=INVALID_HANDLE_VALUE) { FindCloseChangeNotification(change); } return 0;
}
int main(int argc,char** argv)
{
 int failed=0; xide_model_error error={0}; xide_legacy_import *plan=NULL,*second=NULL; xide_workspace workspace={0}; xide_project project={0}; xide_tasks* tasks=NULL; xvalue *old=NULL,*ws=NULL,*report=NULL,*hints=NULL,*json=NULL,*before=NULL; HANDLE lock=INVALID_HANDLE_VALUE,thread=NULL; race raceState={0}; char* owned[100]={0}; size_t ownedCount=0;
#define KEEP(expr) (owned[ownedCount++]=(expr))
 CHECK(argc==2 || argc==3); char* unique=xideCreateId("legacy"); char* root=KEEP(unique ? xidePathResolve(argv[1],unique) : NULL); free(unique); CHECK(root && xrtDirCreateAll(root));
 char *oldDir=KEEP(xidePathResolve(root,"原工程😀")),*sourceDir=KEEP(xidePathResolve(oldDir,"src")),*resourceDir=KEEP(xidePathResolve(oldDir,"res")),*manifest=KEEP(xidePathResolve(oldDir,"旧项目.xlp")),*wsPath=KEEP(xidePathResolve(root,"旧工作空间.xlw")),*destination=KEEP(xidePathResolve(root,"导入新目录😀")),*entry=KEEP(xidePathResolve(sourceDir,"main.xl")),*native=KEEP(xidePathResolve(sourceDir,"native.c")),*header=KEEP(xidePathResolve(sourceDir,"native.h")),*alternate=KEEP(xidePathResolve(sourceDir,"alternate.xl")),*data=KEEP(xidePathResolve(resourceDir,"data.json")),*alias=KEEP(xidePathResolve(sourceDir,"native-alias.c")),*notes=KEEP(xidePathResolve(root,"notes.txt")),*badProject=KEEP(xidePathResolve(root,"bad.xlp"));
 CHECK(oldDir && sourceDir && resourceDir && manifest && wsPath && destination && entry && native && header && alternate && data && alias && notes && badProject && xrtDirCreateAll(sourceDir) && xrtDirCreateAll(resourceDir));
 CHECK(writeText(entry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n    if nativeAnswer() != 42 then\n        return 1\n    end\n    return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n") && writeText(header,"int native_answer(void);\n") && writeText(alternate,"func main(...) as int\n    return 0\nend\n") && writeText(data,"{\"ordinary\":\"用户数据 JSON\"}") && writeText(notes,"workspace loose file\n") && writeText(badProject,"{malformed JSON"));
 wchar_t *a=wide(alias),*b=wide(native); CHECK(a && b && CreateHardLinkW(a,b,NULL)); free(a); free(b);
 old=oldProject(1); CHECK(old && addRef(get(old,"File"),"src/native-alias.c",1) && writeJson(manifest,old));
 ws=xrtJsonParse(sv("{\"Version\":1,\"Name\":\"旧空间😀\",\"NewFileIndex\":7,\"Status\":{\"IsExpand\":false},\"Projects\":[],\"Files\":[],\"FutureWorkspace\":{\"preserve\":true}}"));
 CHECK(ws && addRef(get(ws,"Projects"),"原工程😀/旧项目.xlp",1) && addRef(get(ws,"Projects"),"原工程😀/旧项目.xlp",0) && addRef(get(ws,"Projects"),"missing.xlp",0) && addRef(get(ws,"Projects"),"bad.xlp",0) && addRef(get(ws,"Files"),"notes.txt",1) && addRef(get(ws,"Files"),"missing.txt",0) && writeJson(wsPath,ws));
 xide_file_snapshot sourceBefore={0},sourceAfter={0}; CHECK(xideFileSnapshotRead(manifest,&sourceBefore,&error));
 plan=xideLegacyImportPrepare(wsPath,destination,XIDE_IMPORT_WORKSPACE,&error);
 CHECK(plan && xideLegacyImportProjectCount(plan)==3 && xideLegacyImportEntryCount(plan,0)==2 && !xideLegacyImportReady(plan) && !xidePathExists(destination) && !xideLegacyImportProjectSupported(plan,1) && !xideLegacyImportProjectJson(plan,1));
 CHECK(!xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_INVALID && !xidePathExists(destination));
 report=xrtJsonParse(sv(xideLegacyImportReportJson(plan))); CHECK(report && issueCount(report,"entry-required")==1 && issueCount(report,"duplicate-file")==1 && issueCount(report,"duplicate-project")==1 && issueCount(report,"unavailable-project")==2 && issueCount(report,"library-review")==2 && issueCount(report,"resource-review")==2 && xrtValueEqual(get(report,"original"),ws)); xrtValueRelease(report); report=NULL;
 before=xrtJsonParse(sv(xideLegacyImportProjectJson(plan,0))); CHECK(before && !xideLegacyImportSetEntry(plan,0,"src/native.c",&error)); json=xrtJsonParse(sv(xideLegacyImportProjectJson(plan,0))); CHECK(json && xrtValueEqual(before,json)); xrtValueRelease(before); before=NULL; xrtValueRelease(json); json=NULL;
 CHECK(xideLegacyImportSetEntry(plan,0,entry,&error) && xideLegacyImportReady(plan));
 before=xrtJsonParse(sv(xideLegacyImportProjectJson(plan,0))); CHECK(before);
 json=xrtJsonParse(sv(xideLegacyImportWorkspaceJson(plan))); CHECK(json && string(get(json,"startupProject"),"")); xrtValueRelease(json); json=NULL;
 CHECK(!xideLegacyImportSetStartup(plan,xideLegacyImportProjectId(plan,1),&error) && xideLegacyImportSetStartup(plan,xideLegacyImportProjectId(plan,0),&error));
 CHECK(!xideLegacyImportCommit(plan,0,&error) && !xidePathExists(destination));
 lock=openWrite(manifest); CHECK(lock!=INVALID_HANDLE_VALUE && !xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_IO && !xidePathExists(destination)); CloseHandle(lock); lock=INVALID_HANDLE_VALUE;
 CHECK(xrtDirCreateAll(destination) && !xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_CONFLICT && removeEmpty(destination));
 CHECK(xideLegacyImportCommit(plan,1,&error) && xidePathExists(destination) && stageCount(root)==0 && !xideLegacyImportCommit(plan,1,&error) && !xideLegacyImportSetEntry(plan,0,alternate,&error));
 CHECK(xideFileSnapshotRead(manifest,&sourceAfter,&error) && xideFileSnapshotEqual(&sourceBefore,&sourceAfter) && xideWorkspaceLoad(&workspace,xideLegacyImportWorkspacePath(plan),&error) && workspace.projectCount==3 && workspace.files.count==2 && !workspace.projects[1].available && !workspace.projects[2].available);
 xide_project* p=&workspace.projects[0]; CHECK(p->available && p->files.count==6 && p->cSources.count==1 && p->resources.count==0 && !strcmp(p->entry,entry) && !strcmp(workspace.startupProject,p->id) && xrtValueEqual(get(get(p->source,"ide"),"legacy") ? get(get(get(p->source,"ide"),"legacy"),"original") : NULL,old));
 CHECK(string(get(xrtValueArrayGet(get(get(before,"ide"),"files"),0),"id"),p->files.items[0].id) && string(get(xrtValueArrayGet(get(get(before,"ide"),"files"),1),"id"),p->files.items[1].id) && xrtValueCount(get(p->files.items[1].source,"legacyEntries"))==2); xrtValueRelease(before); before=NULL;
 CHECK(!strcmp(p->configurations[0].workingDirectory,sourceDir)); char* expectedOutput=KEEP(xidePathResolve(oldDir,"bin/imported.exe")); CHECK(expectedOutput && !strcmp(p->configurations[0].output,expectedOutput));
 char *hintsPath=KEEP(xidePathResolve(destination,"session-hints.json")),*reportPath=KEEP(xidePathResolve(destination,"report.json")); CHECK(hintsPath && reportPath);
 hints=xrtJsonParseFile(hintsPath); report=xrtJsonParseFile(reportPath); CHECK(hints && report && xrtValueCount(get(hints,"projects"))==3 && string(get(hints,"format"),"xlang-ide-legacy-session-hints-1") && xrtValueCount(get(xrtValueArrayGet(get(hints,"projects"),0),"files"))==6 && issueCount(report,"entry-required")==0);
 bool mergedOpen=false; CHECK(xrtValueGetBool(get(xrtValueArrayGet(get(xrtValueArrayGet(get(hints,"projects"),0),"files"),1),"open"),&mergedOpen) && mergedOpen);
 CHECK(xideProjectSave(p,NULL,&error)); json=xrtJsonParseFile(p->manifest); CHECK(json && xrtValueEqual(get(get(get(json,"ide"),"legacy"),"original"),old)); xrtValueRelease(json); json=NULL;
 if(argc==3) { tasks=xideTasksCreate(); xide_task_info info={0}; CHECK(tasks); xide_task* task=xideTasksBuild(tasks,p,NULL,argv[2],1,&error); CHECK(task && waitTask(tasks,task,&info)); if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"legacy build: %s\n",info.error); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran); puts("legacy_import_test real DEMO6 passed: imported C/xlang references built and ran."); xideTasksDestroy(tasks); tasks=NULL; }
 xideLegacyImportDestroy(plan); plan=NULL; xideWorkspaceClear(&workspace); xrtValueRelease(hints); hints=NULL; xrtValueRelease(report); report=NULL;
 /* Same bytes at a new physical identity must invalidate review. */
 char* staleTarget=KEEP(xidePathResolve(root,"身份变化目标")); CHECK(staleTarget);
 plan=xideLegacyImportPrepare(manifest,staleTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && xideLegacyImportSetEntry(plan,0,entry,&error) && writeJson(manifest,old) && !xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(staleTarget)); xideLegacyImportDestroy(plan); plan=NULL;
 /* Byte changes with restored last-write time also invalidate review. */
 plan=xideLegacyImportPrepare(manifest,staleTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && xideLegacyImportSetEntry(plan,0,entry,&error));
 lock=openWrite(manifest); FILETIME saved={0}; DWORD written=0; char changed='['; CHECK(lock!=INVALID_HANDLE_VALUE && GetFileTime(lock,NULL,NULL,&saved) && WriteFile(lock,&changed,1,&written,NULL) && written==1 && SetFileTime(lock,NULL,NULL,&saved)); CloseHandle(lock); lock=INVALID_HANDLE_VALUE;
 CHECK(!xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(staleTarget) && writeJson(manifest,old)); xideLegacyImportDestroy(plan); plan=NULL;
 /* Read-only UTF-8 BOM manifests are source material, not rename requests. */
 size_t rawLength=0; char* raw=xrtJsonStringify(old,true,&rawLength); unsigned char* bom=raw ? malloc(rawLength+3) : NULL; CHECK(raw && bom); memcpy(bom,"\xef\xbb\xbf",3); memcpy(bom+3,raw,rawLength); CHECK(xrtFileWriteAtomic(manifest,(xbytesview){bom,rawLength+3})); xrtFree(raw); free(bom);
 CHECK(attributes(manifest,FILE_ATTRIBUTE_READONLY)); plan=xideLegacyImportPrepare(manifest,staleTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && xideLegacyImportSetEntry(plan,0,entry,&error) && xideLegacyImportCommit(plan,1,&error));
 wchar_t* readonly=wide(manifest); CHECK(readonly && (GetFileAttributesW(readonly)&FILE_ATTRIBUTE_READONLY)); free(readonly); CHECK(attributes(manifest,FILE_ATTRIBUTE_NORMAL)); xideLegacyImportDestroy(plan); plan=NULL;
 /* Top-level invalid/mixed/modern inputs and unsafe target names are refused. */
 char *invalid=KEEP(xidePathResolve(root,"invalid.xlp")),*invalidTarget=KEEP(xidePathResolve(root,"无效清单目标")),*reserved=KEEP(xidePathResolve(root,"CON")); CHECK(invalid && invalidTarget && reserved && writeText(invalid,"{\"Version\":1,\"format\":\"xlang-project-1\",\"Project\":{},\"Build\":{},\"File\":[],\"Library\":[]}"));
 CHECK(!xideLegacyImportPrepare(invalid,invalidTarget,XIDE_IMPORT_PROJECT,&error) && error.code==XIDE_MODEL_INVALID && !xidePathExists(invalidTarget) && !xideLegacyImportPrepare(manifest,reserved,XIDE_IMPORT_PROJECT,&error));
 CHECK(writeText(invalid,"{\"Version\":1,\"Project\":{},\"Build\":{\"File\":\"legacy.bin\"},\"File\":[{\"Path\":\"原工程😀/src/main.xl\",\"Open\":true}],\"Library\":[]}"));
 plan=xideLegacyImportPrepare(invalid,invalidTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && !xideLegacyImportReady(plan) && xideLegacyImportSetKind(plan,0,"exe",&error) && xideLegacyImportReady(plan)); xideLegacyImportDestroy(plan); plan=NULL;
 /* A missing referenced project appearing after review invalidates it. */
 char *missingPath=KEEP(xidePathResolve(root,"missing.xlp")),*missingTarget=KEEP(xidePathResolve(root,"缺失状态变化目标")); CHECK(missingPath && missingTarget);
 plan=xideLegacyImportPrepare(wsPath,missingTarget,XIDE_IMPORT_WORKSPACE,&error); CHECK(plan && xideLegacyImportSetEntry(plan,0,entry,&error) && writeText(missingPath,"new arrival") && !xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(missingTarget) && deleteFile(missingPath)); xideLegacyImportDestroy(plan); plan=NULL;
 /* Zero-candidate projects can explicitly choose an external entry. */
 CHECK(writeText(invalid,"{\"Version\":1,\"Project\":{},\"Build\":{},\"File\":[],\"Library\":[]}"));
 plan=xideLegacyImportPrepare(invalid,invalidTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && !xideLegacyImportReady(plan) && xideLegacyImportEntryCount(plan,0)==0 && xideLegacyImportSetEntry(plan,0,entry,&error) && xideLegacyImportReady(plan) && xideLegacyImportEntryCount(plan,0)==1 && !strcmp(xideLegacyImportEntryAt(plan,0,0),entry));
 report=xrtJsonParse(sv(xideLegacyImportReportJson(plan))); CHECK(report && issueCount(report,"entry-added")==1 && issueCount(report,"entry-required")==0); xrtValueRelease(report); report=NULL;
 CHECK(xideLegacyImportCommit(plan,1,&error) && xideWorkspaceLoad(&workspace,xideLegacyImportWorkspacePath(plan),&error) && workspace.projects[0].files.count==1 && !strcmp(workspace.projects[0].entry,entry)); xideLegacyImportDestroy(plan); plan=NULL; xideWorkspaceClear(&workspace);
 /* A well-shaped reference with bad field types stays unavailable. */
 CHECK(writeText(badProject,"{\"Version\":1,\"Project\":{\"Description\":42},\"Build\":{},\"File\":[],\"Library\":[]}"));
 plan=xideLegacyImportPrepare(wsPath,missingTarget,XIDE_IMPORT_WORKSPACE,&error); CHECK(plan && xideLegacyImportProjectCount(plan)==3 && !xideLegacyImportProjectSupported(plan,2)); xideLegacyImportDestroy(plan); plan=NULL;
 /* Raw templates must not be treated as instantiated project paths. */
 CHECK(writeText(invalid,"{\"Version\":1,\"Project\":{\"Description\":\"$Project_Description\",\"AppPath\":\"$Project_AppPath\"},\"Build\":{\"File\":\"$Build_File\"},\"File\":[],\"Library\":[]}"));
 CHECK(!xideLegacyImportPrepare(invalid,missingTarget,XIDE_IMPORT_PROJECT,&error) && error.code==XIDE_MODEL_INVALID);
 /* Parent path spelling staying identical does not accept replaced identity. */
 char *parent=KEEP(xidePathResolve(root,"parent-identity")),*parentBackup=KEEP(xidePathResolve(root,"parent-backup")),*parentTarget=KEEP(xidePathResolve(parent,"target")); CHECK(parent && parentBackup && parentTarget && xrtDirCreateAll(parent));
 plan=xideLegacyImportPrepare(manifest,parentTarget,XIDE_IMPORT_PROJECT,&error); CHECK(plan && xideLegacyImportSetEntry(plan,0,entry,&error));
 wchar_t *parentWide=wide(parent),*backupWide=wide(parentBackup); CHECK(parentWide && backupWide && MoveFileW(parentWide,backupWide) && xrtDirCreateAll(parent));
 CHECK(!xideLegacyImportCommit(plan,1,&error) && error.code==XIDE_MODEL_CONFLICT && !xidePathExists(parentTarget) && removeEmpty(parent) && MoveFileW(backupWide,parentWide)); free(parentWide); free(backupWide); xideLegacyImportDestroy(plan); plan=NULL;
 /* Dynamic capacity: >32 projects, >256 files; no fixed publication array. */
 char *largeWsPath=KEEP(xidePathResolve(root,"large.xlw")),*largeTarget=KEEP(xidePathResolve(root,"large-import")); CHECK(largeWsPath && largeTarget);
 xrtValueRelease(ws); ws=xrtJsonParse(sv("{\"Version\":1,\"Name\":\"容量验证\",\"Projects\":[],\"Files\":[]}")); CHECK(ws);
 for(int i=0;i<40;++i) {
  char name[64]; snprintf(name,sizeof(name),"legacy-%02d.xlp",i); char* path=xidePathResolve(root,name); xvalue* body=xrtJsonParse(sv("{\"Version\":1,\"Project\":{\"Description\":\"capacity\",\"AppPath\":\"build\"},\"Build\":{},\"Library\":[],\"File\":[]}"));
  CHECK(path && body && addRef(get(body,"File"),entry,1));
  for(int j=0;i==0 && j<300;++j) { char file[64]; snprintf(file,sizeof(file),"missing/files-%03d.txt",j); CHECK(addRef(get(body,"File"),file,0)); }
  CHECK(writeJson(path,body) && addRef(get(ws,"Projects"),name,0)); free(path); xrtValueRelease(body);
 }
 CHECK(writeJson(largeWsPath,ws)); plan=xideLegacyImportPrepare(largeWsPath,largeTarget,XIDE_IMPORT_WORKSPACE,&error); CHECK(plan && xideLegacyImportProjectCount(plan)==40 && xideLegacyImportReady(plan));
 /* A competing directory created during staging prevents publication. The
    source is held against writes; only owned staging files are cleaned. */
 raceState.parent=root; raceState.target=largeTarget; raceState.source=largeWsPath; raceState.ready=CreateEventW(NULL,TRUE,FALSE,NULL); CHECK(raceState.ready);
 thread=CreateThread(NULL,0,occupy,&raceState,0,NULL); CHECK(thread && WaitForSingleObject(raceState.ready,5000)==WAIT_OBJECT_0);
 CHECK(!xideLegacyImportCommit(plan,1,&error) && WaitForSingleObject(thread,15000)==WAIT_OBJECT_0 && raceState.created && raceState.locked && xidePathExists(largeTarget) && stageCount(root)==0 && removeEmpty(largeTarget)); CloseHandle(thread); thread=NULL; CloseHandle(raceState.ready); raceState.ready=NULL;
 CHECK(xideLegacyImportCommit(plan,1,&error) && xideWorkspaceLoad(&workspace,xideLegacyImportWorkspacePath(plan),&error) && workspace.projectCount==40 && workspace.projects[0].files.count==301 && workspace.projects[39].available && !strcmp(workspace.startupProject,"")); xideLegacyImportDestroy(plan); plan=NULL; xideWorkspaceClear(&workspace);
 /* Frozen authoritative manifests from the historical native fixture. Paths
    point at copied fixture assets, never at the user's reference repositories. */
 char executable[MAX_PATH*4]={0}; CHECK(GetModuleFileNameA(NULL,executable,sizeof(executable))>0); char* releaseDir=KEEP(xidePathDirectory(executable)); char* ideDir=KEEP(xidePathDirectory(releaseDir)); char* fixtureWs=KEEP(xidePathResolve(ideDir,"tests/fixtures/legacy/workspace/Workspace1.xlw")),*fixtureTarget=KEEP(xidePathResolve(root,"历史清单导入")); CHECK(fixtureWs && fixtureTarget);
 plan=xideLegacyImportPrepare(fixtureWs,fixtureTarget,XIDE_IMPORT_WORKSPACE,&error); CHECK(plan && xideLegacyImportReady(plan) && xideLegacyImportCommit(plan,1,&error) && xideWorkspaceLoad(&workspace,xideLegacyImportWorkspacePath(plan),&error) && workspace.projectCount==1 && workspace.projects[0].cSources.count==1 && workspace.projects[0].resources.count==0 && workspace.files.count==1);
 puts("legacy_import_test passed: authoritative legacy fixture, explicit multi-entry/startup/kind choices, UTF-8 BOM/readonly originals, byte+identity guards, unknown data and session hints, explicit external/zero-candidate entry, placeholder and replaced-parent guards, hardlink dedup, missing/malformed references, resources/library/BootCode reports, no-replace publication race/source locks/staging cleanup, 40 projects/301 files and stable reload.");
cleanup:
 if(thread) { WaitForSingleObject(thread,15000); CloseHandle(thread); } if(raceState.ready) CloseHandle(raceState.ready);
 if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
 xideTasksDestroy(tasks); xideLegacyImportDestroy(plan); xideLegacyImportDestroy(second); xideWorkspaceClear(&workspace); xideProjectClear(&project); xrtValueRelease(old); xrtValueRelease(ws); xrtValueRelease(report); xrtValueRelease(hints); xrtValueRelease(json); xrtValueRelease(before); for(size_t i=0;i<ownedCount;++i) free(owned[i]); return failed;
#undef KEEP
}
