#include "services/build_inputs.h"
#include "services/tasks.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return xrtValueObjectGet(v,sv(k)); }
static int put(xvalue* v,const char* k,xvalue* t) { return t && xrtValueObjectSetNew(v,sv(k),t); }
static int string(xvalue* v,const char* k,const char* s) { return put(v,k,xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)s,strlen(s)}); }
static char* utf8(const wchar_t* w) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,p,n,NULL,NULL); return p; }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* p=n ? malloc((size_t)n*sizeof(*p)) : NULL; if(p) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,p,n); return p; }
static char* environment(const wchar_t* name) { wchar_t w[32768]; DWORD n=GetEnvironmentVariableW(name,w,32768); return n && n<32768 ? utf8(w) : NULL; }
static void emit(const char* s) { DWORD done=0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,(DWORD)strlen(s),&done,NULL); }
static int gate(const char* phase) { char* p=environment(L"XIDE_RECEIPT_GATE"); wchar_t* w=p ? wide(p) : NULL; HANDLE h=w ? OpenEventW(SYNCHRONIZE,FALSE,w) : NULL; free(p); free(w); if(!h) return 0; emit(phase); DWORD n=WaitForSingleObject(h,30000); CloseHandle(h); return n==WAIT_OBJECT_0; }
/* These gates wrap real compilation. Notifications can be consumed before the
   compiler opens the header, and bytes can change after its success receipt. */
static int helper(int count,char** argv) {
 if(count==2 && !strcmp(argv[1],"--help")) { emit("xl build <file.xlp> -o <output> --inc-path --lib-path --link --input-report receipt.json\n"); return 0; }
 if(count<5 || strcmp(argv[1],"build")) return -1;
 char *compiler=environment(L"XIDE_RECEIPT_COMPILER"),*mode=environment(L"XIDE_RECEIPT_MODE"); int okay=compiler!=NULL;
 if(okay && mode && !strcmp(mode,"before")) okay=gate("RECEIPT_BEFORE_BUILD\n");
 xprocess* process=NULL; xprocessstatus status={0};
 if(okay) { xprocessconfig c; xrtProcessConfigInit(&c); c.Program=compiler; c.Args=(const char* const*)argv+1; c.ArgCount=(size_t)count-1; c.HideWindow=true; c.NewGroup=false; c.Stdout.Mode=XPROCESS_IO_INHERIT; c.Stderr.Mode=XPROCESS_IO_INHERIT; process=xrtProcessSpawn(&c); okay=process && xrtProcessWaitFor(process,90*XRT_TIME_SECOND)==XWAIT_OK && xrtProcessStatus(process,&status) && status.Kind==XPROCESS_EXIT_CODE && status.Code==0;
  if(!okay) { char message[1024]; const xerror* e=xrtGetError(); snprintf(message,sizeof(message),"DELEGATE_FAILED kind=%d code=%d error=%s\n",status.Kind,status.Code,e ? xrtErrorMessage(e) : "none"); emit(message); }
 }
 if(process) { if(!okay) xrtProcessKillTree(process); xrtProcessDestroy(process); }
 if(okay && mode && !strcmp(mode,"after")) okay=gate("RECEIPT_AFTER_BUILD\n");
 if(okay && mode && !strcmp(mode,"malformed")) for(int i=1;i+1<count;++i) if(!strcmp(argv[i],"--input-report")) okay=writeText(argv[i+1],"{}");
 free(compiler); free(mode); return okay ? 0 : 3;
}
static int record(const char* path,xvalue* r) { size_t n=0; char* s=xrtJsonStringify(r,false,&n); int okay=s && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)s,n}); xrtFree(s); return okay; }
static int waitTask(xide_tasks* ts,xide_task* t,xide_task_info* i) { uint64 end=GetTickCount64()+30000; do { xideTasksPoll(ts); xideTaskInfo(t,i); if(i->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int waitGate(xide_task* t,const char* phase) { uint64 end=GetTickCount64()+30000,cursor=0,drop=0; char bytes[8192]; size_t at=0; do { size_t n=xideTaskReadLog(t,&cursor,bytes+at,sizeof(bytes)-at-1,&drop); at+=n; bytes[at]=0; if(strstr(bytes,phase)) return 1; xide_task_info i={0}; xideTaskInfo(t,&i); if(i.state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()<end); return 0; }
static int hasInput(xide_task* t,const char* path) { for(size_t i=0;i<xideTaskInputCount(t);++i) { char* p=xideTaskInputPath(t,i); int match=p && xidePathEqual(p,path); free(p); if(match) return 1; } return 0; }
static int sameBytes(const char* path,const unsigned char* saved,size_t size) { size_t n=0; unsigned char* p=xrtFileReadAll(path,&n); int okay=p && n==size && !memcmp(p,saved,n); xrtFree(p); return okay; }
static int replaceSameBytes(const char* path) { xfileinfo before,after; size_t n=0; unsigned char* p=xrtFileReadAll(path,&n); int okay=p && xrtPathStat(path,true,&before) && xrtFileWriteAtomic(path,(xbytesview){p,n}) && xrtPathStat(path,true,&after) && (before.Device!=after.Device || before.Identity!=after.Identity) && sameBytes(path,p,n); xrtFree(p); return okay; }
/* The compiler protocol must retain sub-millisecond timestamps after an XRT
   upgrade. A millisecond conversion would reject this unchanged real input. */
static int stampSubMillisecond(const char* path) {
 wchar_t* w=xidePathNativeWide(path); HANDLE h=w ? CreateFileW(w,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE; free(w);
 if(h==INVALID_HANDLE_VALUE) return 0;
 FILETIME now; GetSystemTimeAsFileTime(&now); ULARGE_INTEGER ticks; ticks.LowPart=now.dwLowDateTime; ticks.HighPart=now.dwHighDateTime;
 ticks.QuadPart=ticks.QuadPart/10000*10000+1234; FILETIME fixed={ticks.LowPart,ticks.HighPart};
 int okay=SetFileTime(h,NULL,NULL,&fixed)!=0; CloseHandle(h); xfileinfo info={0};
 return okay && xideIoReceiptStat(path,&info) && info.Modified%1000==123;
}
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"build_inputs_test:%d: %s; %s; task %s\n",__LINE__,#c,error.message,info.error); failed=1; goto cleanup; } } while(0)
int main(void) {
 int argc=0,failed=0,checks=0; wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&argc); char** argv=args ? calloc((size_t)argc+1,sizeof(*argv)) : NULL;
 if(!argv) { if(args) LocalFree(args); return 2; } for(int i=0;i<argc;++i) argv[i]=utf8(args[i]); LocalFree(args);
 int result=helper(argc,argv); if(result>=0) { for(int i=0;i<argc;++i) free(argv[i]); free(argv); return result; }
 xide_model_error error={0}; xide_task_info info={0}; xide_build_inputs* receipt=NULL; xvalue *original=NULL,*value=NULL; xide_tasks* tasks=NULL; xide_project project={0}; HANDLE event=NULL;
 char *root=NULL,*manifest=NULL,*entry=NULL,*native=NULL,*outside=NULL,*header=NULL,*nested=NULL,*output=NULL,*mutated=NULL,*alias=NULL,*module=NULL,*xentry=NULL,*xmanifest=NULL;
 unsigned char* old=NULL; size_t oldSize=0; char *savedPackage=NULL,*savedCompiler=NULL,*savedMode=NULL,*savedGate=NULL;
 CHECK(argc==3); char unique[80]; snprintf(unique,sizeof(unique),"receipt-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(argv[1],unique); CHECK(root && xrtDirCreateAll(root));
 savedPackage=environment(L"XLANG_PACKAGE_PATH"); savedCompiler=environment(L"XIDE_RECEIPT_COMPILER"); savedMode=environment(L"XIDE_RECEIPT_MODE"); savedGate=environment(L"XIDE_RECEIPT_GATE"); CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL));
 manifest=xidePathResolve(root,"项目.xlp"); entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"native.c"); outside=xidePathResolve(root,"../目录外依赖😀"); header=xidePathResolve(outside,"value.h"); nested=xidePathResolve(outside,"nested.h"); mutated=xidePathResolve(root,"mutated.json"); alias=xidePathResolve(root,"header-alias.h"); CHECK(manifest && entry && native && outside && header && nested && mutated && alias && xrtDirCreateAll(outside));
 CHECK(writeText(header,"#define VALUE 0\n") && writeText(nested,"#include \"value.h\"\n")); char source[4096]; snprintf(source,sizeof(source),"#define PICK \"%s\"\n#include PICK\nint receipt_auxiliary(void) {return VALUE;}\n",nested); CHECK(writeText(native,source) && writeText(entry,"func main(...) as int\n return 0\nend\n"));
 CHECK(stampSubMillisecond(entry));
 CHECK(xideProjectCreate(&project,manifest,"真实输入任务",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error)); output=xidePathResolve(NULL,project.configurations[0].output); CHECK(output); tasks=xideTasksCreate(); CHECK(tasks);
 xide_task* task=xideTasksBuild(tasks,&project,NULL,argv[2],1,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && info.inputReport && info.inputsVerified && hasInput(task,header) && hasInput(task,nested) && xidePathExists(xideTaskInputReceipt(task)));
 const char* realReceipt=xideTaskInputReceipt(task),*artifact=xideTaskArtifact(task); receipt=xideBuildInputsLoad(realReceipt,artifact,NULL,NULL,NULL,&error); CHECK(receipt && xideBuildInputsHasFile(receipt,header) && xideBuildInputsValidate(receipt,&error) && xideBuildInputsArtifactValid(receipt,output,&error));
 CHECK(!xideBuildInputsOutputValid(receipt,header,&error) && !xideBuildInputsOutputValid(receipt,realReceipt,&error) && !xideBuildInputsOutputValid(receipt,artifact,&error));
 original=xrtJsonParseFile(realReceipt); CHECK(original);
 for(int mutation=0;mutation<14;++mutation) {
  value=xrtValueClone(original); CHECK(value); xvalue* item=xrtValueArrayGet(get(value,"files"),0);
  if(mutation==0) CHECK(put(value,"schema",xrtValueInt(2)));
  if(mutation==1) CHECK(put(value,"complete",xrtValueBool(false)));
  if(mutation==2) CHECK(string(value,"scope","loaded-sources-and-manifests"));
  if(mutation==3) CHECK(string(item,"path","relative.bin"));
  if(mutation==4) { char p[4096]; snprintf(p,sizeof(p),"%s/./%s",root,xidePathFilename(entry)); CHECK(string(item,"path",p)); }
  if(mutation==5) CHECK(xrtValueArrayAppend(get(value,"files"),item));
  if(mutation==6) { xvalue* probes=get(value,"nativeMissing"); CHECK(xrtValueCount(probes) && xrtValueArrayAppend(probes,xrtValueArrayGet(probes,0))); }
  if(mutation==7) { xvalue* probe=xrtValueObject(); CHECK(probe && string(probe,"path",header) && put(probe,"exists",xrtValueBool(false)) && xrtValueArrayAppendNew(get(value,"xlangProbes"),probe)); }
  if(mutation==8) CHECK(string(item,"sha256","0000000000000000000000000000000000000000000000000000000000000000"));
  if(mutation==9) CHECK(string(get(item,"metadata"),"modifiedUs","01"));
  if(mutation==10) CHECK(put(item,"size",xrtValueInt(64u*1024u*1024u+1)));
  if(mutation==11) CHECK(put(value,"packagePathObserved",xrtValueBool(true)) && string(value,"XLANG_PACKAGE_PATH","different"));
  if(mutation==12) CHECK(string(get(value,"artifact"),"path",output));
  if(mutation==13) CHECK(put(value,"unknown",xrtValueBool(true)));
  CHECK(record(mutated,value) && !xideBuildInputsLoad(mutated,artifact,NULL,NULL,NULL,&error)); xrtValueRelease(value); value=NULL;
 }
 size_t size=0; char* s=xrtJsonStringify(original,false,&size); CHECK(s && s[0]=='{'); char* duplicate=malloc(size+32); CHECK(duplicate); snprintf(duplicate,size+32,"{\"schema\":1,%s",s+1); int rejected=writeText(mutated,duplicate) && !xideBuildInputsLoad(mutated,artifact,NULL,NULL,NULL,&error); CHECK(rejected);
 char* schema=strstr(s,"\"schema\":1"); CHECK(schema); size_t at=(size_t)(schema-s)+strlen("\"schema\":1"); memcpy(duplicate,s,at); memcpy(duplicate+at,".0",2); strcpy(duplicate+at+2,s+at); rejected=writeText(mutated,duplicate) && !xideBuildInputsLoad(mutated,artifact,NULL,NULL,NULL,&error); free(duplicate); xrtFree(s); CHECK(rejected);
 wchar_t *aw=wide(alias),*hw=wide(header); int linked=aw && hw && CreateHardLinkW(aw,hw,NULL); free(aw); free(hw); CHECK(linked && !xideBuildInputsOutputValid(receipt,alias,&error));
 uint32_t commands=0,options=0; const char* help="xl run <file> --input-report a\nxl build <file> -o out --input-reporting bad\n"; CHECK(xideToolchainParseHelp(help,strlen(help),&commands,&options,&error) && !(options&XIDE_TOOL_BUILD_INPUT_REPORT)); help="xl build <file> -o out [--input-report receipt.json]\n"; CHECK(xideToolchainParseHelp(help,strlen(help),&commands,&options,&error) && (options&XIDE_TOOL_BUILD_INPUT_REPORT));
 old=xrtFileReadAll(output,&oldSize); CHECK(old && oldSize);
 xide_strings dirty={.items=&header,.count=1}; task=xideTasksBuildGuarded(tasks,&project,NULL,argv[2],1,&dirty,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran && sameBytes(output,old,oldSize));
 dirty.items=&alias; task=xideTasksBuildGuarded(tasks,&project,NULL,argv[2],1,&dirty,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran && sameBytes(output,old,oldSize));
 wchar_t gateName[128]; swprintf(gateName,128,L"Local\\xide-receipt-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); event=CreateEventW(NULL,TRUE,FALSE,gateName); wchar_t* compilerW=wide(argv[2]); CHECK(event && compilerW && SetEnvironmentVariableW(L"XIDE_RECEIPT_GATE",gateName) && SetEnvironmentVariableW(L"XIDE_RECEIPT_COMPILER",compilerW)); free(compilerW);
 CHECK(SetEnvironmentVariableW(L"XIDE_RECEIPT_MODE",L"before")); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_BEFORE_BUILD")); xideTasksInvalidateInput(tasks,header); Sleep(50); CHECK(SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran && sameBytes(output,old,oldSize));
 CHECK(ResetEvent(event) && SetEnvironmentVariableW(L"XIDE_RECEIPT_MODE",L"after")); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_AFTER_BUILD")); CHECK(writeText(header,"#define VALUE 1\n") && SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize));
 CHECK(writeText(header,"#define VALUE 0\n") && ResetEvent(event)); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_AFTER_BUILD")); CHECK(replaceSameBytes(header) && SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize));
 CHECK(ResetEvent(event)); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_AFTER_BUILD")); CHECK(replaceSameBytes(xideTaskArtifact(task)) && SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize));
 CHECK(ResetEvent(event)); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_AFTER_BUILD")); char* missingSource=xidePathResolve(root,"main.c"); value=xrtJsonParseFile(xideTaskInputReceipt(task)); int observedMissing=0; CHECK(missingSource && value); const xvalue* candidates=get(value,"nativeMissing"); for(size_t i=0;i<xrtValueCount(candidates);++i) { xstrview candidate={0}; if(xrtValueGetString(xrtValueArrayGet(candidates,i),&candidate) && xidePathLexicalEqual(candidate.Data,missingSource)) observedMissing=1; } CHECK(observedMissing && writeText(missingSource,"int appeared_after_compilation;\n") && SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize)); CHECK(xrtFileDelete(missingSource)); free(missingSource); xrtValueRelease(value); value=NULL;
 CHECK(writeText(header,"#define VALUE 0\n") && SetEnvironmentVariableW(L"XIDE_RECEIPT_MODE",L"malformed")); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize));
 CHECK(ResetEvent(event) && SetEnvironmentVariableW(L"XIDE_RECEIPT_MODE",L"after")); task=xideTasksBuild(tasks,&project,NULL,argv[0],1,&error); CHECK(task && waitGate(task,"RECEIPT_AFTER_BUILD")); CHECK(writeText(xideTaskArtifact(task),"replaced artifact") && SetEvent(event) && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(output,old,oldSize));
 free(project.configurations[0].output); project.configurations[0].output=xidePathResolve(NULL,header); CHECK(project.configurations[0].output && xideProjectSave(&project,NULL,&error)); CHECK(SetEnvironmentVariableW(L"XIDE_RECEIPT_MODE",NULL)); task=xideTasksBuild(tasks,&project,NULL,argv[2],0,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && sameBytes(header,(unsigned char*)"#define VALUE 0\n",strlen("#define VALUE 0\n")));
 xideProjectClear(&project); xmanifest=xidePathResolve(root,"xlang.xlp"); xentry=xidePathResolve(root,"main.xl"); module=xidePathResolve(root,"dep.xl"); CHECK(xmanifest && xentry && module && writeText(xentry,"import dep\nfunc main(...) as int\n return dep.answer()\nend\n") && writeText(module,"export func answer() as int\n return 0\nend\n") && xideProjectCreate(&project,xmanifest,"xlang 导入任务",xentry,&error) && xideProjectSave(&project,NULL,&error));
 task=xideTasksBuild(tasks,&project,NULL,argv[2],1,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.inputsVerified && info.ran && hasInput(task,module)); dirty.items=&module; task=xideTasksBuildGuarded(tasks,&project,NULL,argv[2],1,&dirty,&error); CHECK(task && waitTask(tasks,task,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran);
cleanup:
 if(event) { SetEvent(event); } xideTasksDestroy(tasks); if(event) { CloseHandle(event); } xideProjectClear(&project); xideBuildInputsDestroy(receipt); xrtValueRelease(original); xrtValueRelease(value); xrtFree(old);
 const wchar_t* names[]={L"XLANG_PACKAGE_PATH",L"XIDE_RECEIPT_COMPILER",L"XIDE_RECEIPT_MODE",L"XIDE_RECEIPT_GATE"}; char* saved[]={savedPackage,savedCompiler,savedMode,savedGate}; for(int i=0;i<4;++i) { wchar_t* w=saved[i] ? wide(saved[i]) : NULL; SetEnvironmentVariableW(names[i],w); free(w); free(saved[i]); }
 char** paths[]={&root,&manifest,&entry,&native,&outside,&header,&nested,&output,&mutated,&alias,&module,&xentry,&xmanifest}; for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) free(*paths[i]);
 for(int i=0;i<argc;++i) { free(argv[i]); } free(argv); printf("Build input IDE checks: %d %s\n",checks,failed ? "failed" : "passed"); return failed ? 1 : 0;
}
