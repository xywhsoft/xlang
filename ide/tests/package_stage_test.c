#include "services/tasks.h"
#include "platform/task_stage.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"package_stage_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,info.error); failed=1; goto done; } } while(0)
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static char* utf8(const wchar_t* text)
{
 int size=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL); char* out=size ? malloc((size_t)size) : NULL;
 if(out) WideCharToMultiByte(CP_UTF8,0,text,-1,out,size,NULL,NULL);
 return out;
}
static wchar_t* wide(const char* text)
{
 int size=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0); wchar_t* out=size ? malloc((size_t)size*sizeof(*out)) : NULL;
 if(out) MultiByteToWideChar(CP_UTF8,0,text,-1,out,size);
 return out;
}
static wchar_t* environment(const wchar_t* name)
{
 DWORD size=GetEnvironmentVariableW(name,NULL,0); if(!size) return NULL;
 wchar_t* out=calloc(size,sizeof(*out)); if(out && !GetEnvironmentVariableW(name,out,size)) { free(out); out=NULL; } return out;
}
static void emit(const char* text)
{ DWORD count=0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)strlen(text),&count,NULL); }
/* An actual subprocess reports its package/CWD observations, then waits without
   opening any cached file. Protection therefore comes from the task service. */
static int compilerHelper(int argc,char** argv)
{
 if(argc==2 && !strcmp(argv[1],"--help")) { emit("xl build <file.xlp> -o <output>\n"); return 0; }
 if(argc<5 || strcmp(argv[1],"build")) return -1;
 wchar_t *readyName=environment(L"XIDE_PACKAGE_STAGE_READY"),*finishName=environment(L"XIDE_PACKAGE_STAGE_FINISH"),*pkg=environment(L"XLANG_PACKAGE_PATH");
 HANDLE ready=readyName ? OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName) : NULL,finish=finishName ? OpenEventW(SYNCHRONIZE,FALSE,finishName) : NULL;
 wchar_t cwd[32768]; DWORD size=GetCurrentDirectoryW(32768,cwd); char* text=size && size<32768 ? utf8(cwd) : NULL;
 if(!ready || !finish || !pkg || !text) return 4;
 emit("CWD["); emit(text); emit("]\n"); free(text); text=utf8(pkg); if(!text) return 5;
 emit("PACKAGE["); emit(text); emit("]\n"); free(text);
 wchar_t* next=pkg;
 while(next) {
  wchar_t* end=wcschr(next,L';'); if(end) *end=0;
  if(*next) {
   wchar_t resolved[32768]; DWORD n=GetFullPathNameW(next,32768,resolved,NULL); text=n && n<32768 ? utf8(resolved) : NULL;
   if(!text) return 6;
   emit("RESOLVED["); emit(text); emit("]\n"); free(text);
  }
  next=end ? end+1 : NULL;
 }
 emit("PACKAGE_HELPER_READY\n");
 int okay=SetEvent(ready) && WaitForSingleObject(finish,15000)==WAIT_OBJECT_0;
 CloseHandle(ready); CloseHandle(finish); free(readyName); free(finishName); free(pkg);
 /* Deliberate failure still needs history protection; nothing is published. */
 return okay ? 9 : 7;
}
static int owned(const char* directory,const char* body,xide_model_error* error)
{
 xide_task_stage* owner=xideTaskStageBegin(directory,error); if(!owner) return 0;
 int okay=xideIoWriteAtomic(body,(xbytesview){(const unsigned char*)"owned bytes\n",12}) && xideTaskStageComplete(owner,error); xideTaskStageRelease(owner); return okay;
}
static int awaitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info)
{
 ULONGLONG limit=GetTickCount64()+15000;
 do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<limit); return 0;
}
static int hasResolved(const char* log,const char* expected)
{
 const char* item=log;
 while((item=strstr(item,"RESOLVED["))!=NULL) {
  item+=9; const char* end=strchr(item,']'); if(!end) return 0;
  size_t size=(size_t)(end-item); char* value=malloc(size+1); if(!value) return 0;
  memcpy(value,item,size); value[size]=0; int same=xidePathLexicalEqual(value,expected); free(value); if(same) return 1;
  item=end+1;
 }
 return 0;
}
int main(void)
{
 int argc=0; wchar_t** argvWide=CommandLineToArgvW(GetCommandLineW(),&argc); char** argv=calloc((size_t)argc+1,sizeof(*argv));
 if(!argvWide || !argv) { if(argvWide) LocalFree(argvWide); free(argv); return 1; }
 for(int i=0;i<argc;++i) { argv[i]=utf8(argvWide[i]); if(!argv[i]) { LocalFree(argvWide); for(int j=0;j<i;++j) free(argv[j]); free(argv); return 1; } } LocalFree(argvWide);
 int helper=compilerHelper(argc,argv); if(helper>=0) { for(int i=0;i<argc;++i) free(argv[i]); free(argv); return helper; }
 int checks=0,failed=0; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0}; xide_tasks* tasks=NULL; xvalue *configs=NULL,*run=NULL;
 char *root=NULL,*consumer=NULL,*manifest=NULL,*entry=NULL,*shell=NULL,*cacheA=NULL,*cacheB=NULL,*cacheC=NULL,*stageA=NULL,*stageB=NULL,*stageC=NULL,*bodyA=NULL,*bodyB=NULL,*bodyC=NULL,*package=NULL;
 HANDLE ready=NULL,finish=NULL; wchar_t originalCwd[32768],readyName[160],finishName[160];
 wchar_t *oldPackage=environment(L"XLANG_PACKAGE_PATH"),*oldReady=environment(L"XIDE_PACKAGE_STAGE_READY"),*oldFinish=environment(L"XIDE_PACKAGE_STAGE_FINISH"),*shellWide=NULL,*packageWide=NULL;
 DWORD cwdSize=GetCurrentDirectoryW(32768,originalCwd); int changedCwd=0;
 int driveRelative=argc==3 && !strcmp(argv[2],"--drive-relative");
 CHECK((argc==2 || driveRelative) && cwdSize && cwdSize<32768); root=xidePathResolveLexical(NULL,argv[1]); CHECK(root);
 consumer=xidePathResolveLexical(root,"consumer 中文😀"); shell=xidePathResolveLexical(root,"shell/current");
 manifest=consumer ? xidePathResolveLexical(consumer,"project.xlp") : NULL; entry=consumer ? xidePathResolveLexical(consumer,"main.xl") : NULL;
 cacheA=xidePathResolveLexical(root,"old-output 中文😀/.xlang-ide-build"); cacheB=xidePathResolveLexical(root,"absolute-output/.xlang-ide-build"); cacheC=xidePathResolveLexical(root,"shell/old-output 中文😀/.xlang-ide-build");
 CHECK(consumer && shell && manifest && entry && cacheA && cacheB && cacheC && xideIoCreateDirectories(consumer) && xideIoCreateDirectories(shell));
 const char* guid="01234567-89ab-cdef-0123-456789abcdef";
 stageA=xidePathResolveLexical(cacheA,guid); stageB=xidePathResolveLexical(cacheB,guid); stageC=xidePathResolveLexical(cacheC,guid);
 bodyA=stageA ? xidePathResolveLexical(stageA,"request.xlp") : NULL; bodyB=stageB ? xidePathResolveLexical(stageB,"request.xlp") : NULL; bodyC=stageC ? xidePathResolveLexical(stageC,"request.xlp") : NULL;
 CHECK(stageA && stageB && stageC && bodyA && bodyB && bodyC && owned(stageA,bodyA,&error) && owned(stageB,bodyB,&error) && owned(stageC,bodyC,&error));
 const char* source="func main(...) as int\n return 0\nend\n";
 CHECK(xideIoWriteAtomic(entry,(xbytesview){(const unsigned char*)source,strlen(source)}) && xideProjectCreate(&project,manifest,"包路径保护",entry,&error));
 configs=xideProjectConfigurationsJson(&project); run=xrtJsonParse(sv("{\"workingDirectory\":\"../shell/current\"}"));
 CHECK(configs && run); int set=xrtValueObjectSetNew(xrtValueArrayGet(configs,0),sv("run"),run); run=NULL;
 CHECK(set && xideProjectApplyConfigurations(&project,configs,project.activeConfiguration,&error) && xideProjectSave(&project,NULL,&error) && xidePathEqual(project.configurations[0].workingDirectory,shell));
 const char* relative="../old-output 中文😀/.xlang-ide-build/01234567-89ab-cdef-0123-456789abcdef";
 char drivePath[512];
 if(driveRelative) { snprintf(drivePath,sizeof(drivePath),"%c:%s",consumer[0],relative); relative=drivePath; }
 size_t length=strlen(relative)*2+strlen(stageB)+64; package=malloc(length); CHECK(package);
 snprintf(package,length,";missing-base;%s;;%s;%s;",relative,stageB,relative); packageWide=wide(package); shellWide=xidePathNativeWide(shell); CHECK(packageWide && shellWide);
 swprintf(readyName,160,L"Local\\XidePackageReady-%lu-%llu",(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 swprintf(finishName,160,L"Local\\XidePackageFinish-%lu-%llu",(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 ready=CreateEventW(NULL,TRUE,FALSE,readyName); finish=CreateEventW(NULL,TRUE,FALSE,finishName);
 CHECK(ready && finish && SetEnvironmentVariableW(L"XIDE_PACKAGE_STAGE_READY",readyName) && SetEnvironmentVariableW(L"XIDE_PACKAGE_STAGE_FINISH",finishName) && SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",packageWide));
 CHECK(SetCurrentDirectoryW(shellWide)); changedCwd=1;
 tasks=xideTasksCreate(); xide_task* task=tasks ? xideTasksBuild(tasks,&project,NULL,argv[0],0,&error) : NULL; CHECK(task);
 CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L"changed-after-submit") && WaitForSingleObject(ready,10000)==WAIT_OBJECT_0);
 FILETIME time={0}; GetSystemTimeAsFileTime(&time); uint64_t future=(((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime)+XIDE_TASK_STAGE_GRACE_TICKS+UINT64_C(86400)*10000000; xide_task_stage_stats stats={0};
 CHECK(xideTaskStagePrune(cacheA,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(bodyA));
 CHECK(xideTaskStagePrune(cacheB,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(bodyB));
 CHECK(xideTaskStagePrune(cacheC,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xideIoExists(stageC));
 CHECK(SetEvent(finish) && awaitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.exit.Code==9 && !info.built);
 uint64_t cursor=0,dropped=0; char log[16384]; size_t count=xideTaskReadLog(task,&cursor,log,sizeof(log)-1,&dropped); log[count]=0;
 CHECK(!dropped && strstr(log,package) && !strstr(log,"changed-after-submit") && hasResolved(log,stageA) && hasResolved(log,stageB));
 const char* cwd=strstr(log,"CWD["); CHECK(cwd); const char* cwdEnd=strchr(cwd+4,']'); CHECK(cwdEnd);
 size_t n=(size_t)(cwdEnd-(cwd+4)); char* observed=malloc(n+1); CHECK(observed); memcpy(observed,cwd+4,n); observed[n]=0;
 int matches=xidePathLexicalEqual(observed,consumer) && !xidePathLexicalEqual(observed,shell); free(observed); CHECK(matches);
 CHECK(xideTaskStagePrune(cacheA,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(bodyA));
 CHECK(xideTaskStagePrune(cacheB,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(bodyB));
 xideTasksDestroy(tasks); tasks=NULL;
 CHECK(xideTaskStagePrune(cacheA,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xideIoExists(stageA));
 CHECK(xideTaskStagePrune(cacheB,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xideIoExists(stageB) && xideIoExists(entry) && xideIoExists(manifest));
 printf("Package stage checks: %d passed; compiler/project CWD vs IDE/run CWD, Unicode relative and absolute lists, empty/duplicate entries, captured raw environment, active/completed history and unrelated shadow cleanup\n",checks);
done:
 if(finish) SetEvent(finish);
 xideTasksDestroy(tasks); xideProjectClear(&project); xrtValueRelease(configs); xrtValueRelease(run);
 if(changedCwd) SetCurrentDirectoryW(originalCwd);
 SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",oldPackage); SetEnvironmentVariableW(L"XIDE_PACKAGE_STAGE_READY",oldReady); SetEnvironmentVariableW(L"XIDE_PACKAGE_STAGE_FINISH",oldFinish);
 if(ready) CloseHandle(ready);
 if(finish) CloseHandle(finish);
 free(oldPackage); free(oldReady); free(oldFinish); free(shellWide); free(packageWide);
 char* paths[]={root,consumer,manifest,entry,shell,cacheA,cacheB,cacheC,stageA,stageB,stageC,bodyA,bodyB,bodyC,package}; for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) free(paths[i]);
 for(int i=0;i<argc;++i) free(argv[i]);
 free(argv); return failed;
}
