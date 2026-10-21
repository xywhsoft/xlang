#define _WIN32_WINNT 0x0601
#include "services/tasks.h"
#include "services/build_inputs.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,sv(key)); }
static char* utf8(const wchar_t* w) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL); char* s=n ? malloc((size_t)n) : NULL; if(s) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,s,n,NULL,NULL); return s; }
static size_t length(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); return n>0 ? (size_t)n-1 : 0; }
static int writeText(const char* path,const char* text) { return xideIoWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static xvalue* readJson(const char* path)
{ size_t n=0; unsigned char* s=xideIoReadAllLimit(path,32u*1024u*1024u,&n); xvalue* v=s ? xrtJsonParse((xstrview){(char*)s,n}) : NULL; xrtFree(s); return v; }
static int helper(int argc,char** argv)
{
 if(argc==2 && !strcmp(argv[1],"--help")) { puts("xl build <file.xlp> -o <output>"); return 0; }
 if(argc<5 || strcmp(argv[1],"build")) return -1;
 xvalue* snapshot=readJson(argv[2]); xstrview entry={0};
 int okay=snapshot && xrtValueGetString(get(snapshot,"entry"),&entry) && xidePathExists(entry.Data);
 char* directory=xidePathDirectory(argv[0]); char* program=directory ? xidePathResolve(directory,"xide_task_program.exe") : NULL;
 okay=okay && program && xideIoCopy(program,argv[4],1);
 free(program); free(directory); xrtValueRelease(snapshot); return okay ? 0 : 3;
}
static int waitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info)
{
 uint64_t end=GetTickCount64()+30000;
 do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(xideTaskInputOpen(task)) xideTaskCloseInput(task);
      if(info->state>=XIDE_TASK_SUCCEEDED) { return 1; } Sleep(5); } while(GetTickCount64()<end);
 return 0;
}
static int waitProcess(xide_process* p,xide_process_info* info,char* text,size_t capacity)
{
 size_t at=0; uint64_t end=GetTickCount64()+10000;
 do { size_t n=xideProcessRead(p,text+at,capacity-at-1); at+=n; text[at]=0;
      xideProcessGetInfo(p,info); if(info->state==XIDE_PROCESS_FINISHED) return 1; Sleep(5); } while(GetTickCount64()<end);
 return 0;
}
static char* atLength(const char* base,size_t wanted)
{
 char* id=xideCreateId("路径😀"); char* root=id ? xidePathResolveLexical(base,id) : NULL; free(id);
 if(!root || length(root)+2>wanted) { free(root); return NULL; }
 while(length(root)+49<wanted) { char* next=xidePathResolveLexical(root,"nested-abcdefghijklmnopqrstuvwxyz-0123456789"); free(root); root=next; if(!root) return NULL; }
 size_t n=wanted-length(root)-1; char name[128]; if(n>=sizeof(name)) { free(root); return NULL; }
 memset(name,'p',n); name[n]=0; char* next=xidePathResolveLexical(root,name); free(root); return next;
}
static int ordinary(const char* s) { return s && strncmp(s,"//?/",4) && strncmp(s,"\\\\?\\",4); }
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"task_path_test:%d: %s; %s; task=%s\n",__LINE__,#v,error.message,info.error); failed=1; goto cleanup; } } while(0)
int main(void)
{
 int argc=0; wchar_t** w=CommandLineToArgvW(GetCommandLineW(),&argc); char** argv=w ? calloc((size_t)argc+1,sizeof(*argv)) : NULL;
 if(!argv) { if(w) LocalFree(w); return 2; } for(int i=0;i<argc;++i) argv[i]=utf8(w[i]); LocalFree(w);
 int result=helper(argc,argv); if(result>=0) { for(int i=0;i<argc;++i) free(argv[i]); free(argv); return result; }
 int checks=0,failed=0; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0},round={0};
 xide_tasks* tasks=NULL; xide_build_inputs* receipt=NULL; xide_process* process=NULL; xvalue* json=NULL;
 HANDLE locked=INVALID_HANDLE_VALUE; unsigned char *old=NULL,*now=NULL; size_t oldSize=0,nowSize=0;
 char *root=NULL,*manifest=NULL,*entry=NULL,*native=NULL,*output=NULL,*stage=NULL,*snapshot=NULL,*invocation=NULL,
      *deep=NULL,*deepManifest=NULL,*deepReceipt=NULL,*deepArtifact=NULL,*deepAlias=NULL,*deepProgram=NULL,*deepLog=NULL,*selfDir=NULL,*selfProgram=NULL,*package=NULL;
 CHECK(argc==2 || argc==3);
 CHECK(xideBuildInputsPackagePath(&package,&error));
 root=atLength(argv[1],170); CHECK(root && length(root)==170 && xideIoCreateDirectories(root));
 manifest=xidePathResolveLexical(root,"project.xlp"); entry=xidePathResolveLexical(root,"main.xl"); native=xidePathResolveLexical(root,"native.c"); CHECK(manifest && entry && native);
 CHECK(writeText(entry,"extern \"deep_native\" func deepNative() as c_int\nfunc main(...) as int\n if deepNative() != 42 then\n  return 1\n end\n return 0\nend\n") && writeText(native,"int deep_native(void) { return 42; }\n"));
 CHECK(xideProjectCreate(&project,manifest,"原生任务路径",entry,&error) && xideProjectAddFile(&project,native,"c",&error));
 CHECK(xrtValueObjectSetNew(project.configurations[0].environment,sv("XIDE_TASK_MODE"),xrtValueString(sv(""))) && xideProjectSave(&project,NULL,&error));
 tasks=xideTasksCreate(); CHECK(tasks); const char* compiler=argc==3 ? argv[2] : argv[0];
 xide_task* first=xideTasksBuild(tasks,&project,NULL,compiler,1,&error); CHECK(first && waitTask(tasks,first,&info));
 CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran);
 if(argc==3) CHECK(info.inputReport && info.inputsVerified);
 output=xidePathResolveLexical(NULL,xideTaskOutput(first)); stage=xidePathDirectory(xideTaskArtifact(first));
 snapshot=stage ? xidePathResolveLexical(stage,"request.xlp") : NULL; invocation=stage ? xidePathResolveLexical(stage,"build-invocation.json") : NULL;
 CHECK(output && snapshot && invocation && length(snapshot)>240 && length(snapshot)<260 && length(invocation)>=260 && length(xideTaskBuildLog(first))>=250);
 json=readJson(snapshot); xstrview path={0}; CHECK(json && xrtValueGetString(get(json,"entry"),&path) && ordinary(path.Data) && !strcmp(path.Data,entry)); xrtValueRelease(json); json=NULL;
 json=readJson(invocation); CHECK(json && xrtValueGetString(get(json,"compiler"),&path) && ordinary(path.Data) && xidePathEqual(path.Data,compiler)); xrtValueRelease(json); json=NULL;
 CHECK(xidePathExists(xideTaskBuildLog(first)) && xidePathExists(xideTaskRunLog(first)));
 old=xideIoReadAllLimit(output,16u*1024u*1024u,&oldSize); CHECK(old && oldSize>1000);
 wchar_t* target=xidePathNativeWide(output); locked=target ? CreateFileW(target,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL) : INVALID_HANDLE_VALUE; free(target); CHECK(locked!=INVALID_HANDLE_VALUE);
 xide_task* blocked=xideTasksBuild(tasks,&project,NULL,compiler,1,&error); CHECK(blocked && waitTask(tasks,blocked,&info));
 CHECK(info.state==XIDE_TASK_FAILED && !info.built && !info.ran);
 now=xideIoReadAllLimit(output,16u*1024u*1024u,&nowSize); CHECK(now && nowSize==oldSize && !memcmp(old,now,oldSize)); xrtFree(now); now=NULL;
 CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
 deep=atLength(argv[1],430); CHECK(deep && length(deep)==430 && xideIoCreateDirectories(deep));
 deepManifest=xidePathResolveLexical(deep,"project.xlp"); deepReceipt=xidePathResolveLexical(deep,"inputs.json"); deepArtifact=xidePathResolveLexical(deep,"artifact.exe"); deepAlias=xidePathResolveLexical(deep,"source-alias.xl");
 CHECK(deepManifest && deepReceipt && deepArtifact && deepAlias && xideProjectLoad(&round,manifest,&error) && xideProjectSave(&round,deepManifest,&error));
 xideProjectClear(&round); CHECK(xideProjectLoad(&round,deepManifest,&error) && xidePathEqual(round.entry,entry));
 if(argc==3) {
  CHECK(xideIoCopy(xideTaskInputReceipt(first),deepReceipt,0) && xideIoCopy(xideTaskArtifact(first),deepArtifact,0));
  receipt=xideBuildInputsLoad(deepReceipt,xideTaskArtifact(first),package,NULL,NULL,&error);
  CHECK(receipt && xideBuildInputsValidate(receipt,&error) && xideBuildInputsArtifactValid(receipt,deepArtifact,&error));
  CHECK(!xideBuildInputsOutputValid(receipt,deepReceipt,&error) && xideBuildInputsOutputValid(receipt,deepArtifact,&error));
  wchar_t *alias=xidePathNativeWide(deepAlias),*source=xidePathNativeWide(entry); int linked=alias && source && CreateHardLinkW(alias,source,NULL); free(alias); free(source);
  CHECK(linked && !xideBuildInputsOutputValid(receipt,deepAlias,&error));
 }
 selfDir=xidePathDirectory(argv[0]); selfProgram=selfDir ? xidePathResolve(selfDir,"xide_task_program.exe") : NULL;
 deepProgram=xidePathResolveLexical(deep,"program.exe"); deepLog=xidePathResolveLexical(deep,"stdout.log");
 CHECK(selfProgram && deepProgram && deepLog && xideIoCopy(selfProgram,deepProgram,0));
 const char* arguments[]={"中文😀 参数","a\"b","ends\\"}; xprocessenv env={"XIDE_TASK_MODE",""};
 xide_process_spec request={.program=deepProgram,.directory=root,.logPath=deepLog,.arguments=arguments,.argumentCount=3,.environment=&env,.environmentCount=1,.separateStderr=1};
 char processError[512]={0},text[4096]={0}; xide_process_info processInfo={0};
 process=xideProcessStart(&request,processError,sizeof(processError)); CHECK(process && waitProcess(process,&processInfo,text,sizeof(text)));
 if(processInfo.ioError) fprintf(stderr,"deep process: %s\n",processInfo.error);
 CHECK(processInfo.started && !processInfo.ioError && processInfo.exit.Kind==XPROCESS_EXIT_CODE && processInfo.exit.Code==0 && strstr(text,"RUN_STARTED") && strstr(text,"中文😀 参数") && strstr(text,"ARG[a\"b]") && strstr(text,"ARG[ends\\]"));
 CHECK(xidePathExists(deepLog));
 printf("Task path checks: %d passed; snapshot=%zu invocation=%zu deep=%zu UTF16; realCompiler=%d\n",checks,length(snapshot),length(invocation),length(deep),argc==3);
cleanup:
 if(locked!=INVALID_HANDLE_VALUE) { CloseHandle(locked); } xideProcessDestroy(process); xideTasksDestroy(tasks); xideBuildInputsDestroy(receipt); xideProjectClear(&project); xideProjectClear(&round); xrtValueRelease(json); xrtFree(old); xrtFree(now);
 char** paths[]={&root,&manifest,&entry,&native,&output,&stage,&snapshot,&invocation,&deep,&deepManifest,&deepReceipt,&deepArtifact,&deepAlias,&deepProgram,&deepLog,&selfDir,&selfProgram,&package};
 for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) free(*paths[i]);
 for(int i=0;i<argc;++i) { free(argv[i]); } free(argv); return failed ? 1 : 0;
}
