#include "platform/task_stage.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"task_stage_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto done; } } while(0)
static int leaseUnavailable(const char* path)
{
 wchar_t* wide=xidePathNativeWide(path); HANDLE h=wide ? CreateFileW(wide,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
 DWORD status=GetLastError(); free(wide); if(h!=INVALID_HANDLE_VALUE) { CloseHandle(h); return 0; } return status==ERROR_SHARING_VIOLATION;
}
static uint64_t nowTicks(void) { FILETIME t={0}; GetSystemTimeAsFileTime(&t); return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int cancelNow(void* user) { (void)user; return 1; }
static int moveFixtureDirectory(const char* source,const char* target)
{
 wchar_t *from=xidePathNativeWide(source),*to=xidePathNativeWide(target);
 int okay=from && to && MoveFileExW(from,to,0);
 if(!okay) fprintf(stderr,"Directory move Win32=%lu\n",(unsigned long)GetLastError());
 free(from); free(to); return okay;
}
static int editPreservingMetadata(const char* path,unsigned char byte)
{
 wchar_t* wide=xidePathNativeWide(path); HANDLE h=wide ? CreateFileW(wide,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; free(wide); if(h==INVALID_HANDLE_VALUE) return 0;
 BY_HANDLE_FILE_INFORMATION info={0}; DWORD written=0;
 int okay=GetFileInformationByHandle(h,&info) && WriteFile(h,&byte,1,&written,NULL) && written==1 && FlushFileBuffers(h) && SetFileTime(h,&info.ftCreationTime,NULL,&info.ftLastWriteTime); CloseHandle(h); return okay;
}
static int readerChild(int argc,char** argv)
{
 if(argc!=5 || strcmp(argv[1],"--lease")) return -1;
 xide_model_error error={0}; xide_task_stage* lease=xideTaskStageProtectPath(argv[2],&error);
 wchar_t readyName[160],exitName[160];
 if(!lease || !MultiByteToWideChar(CP_UTF8,0,argv[3],-1,readyName,160) || !MultiByteToWideChar(CP_UTF8,0,argv[4],-1,exitName,160)) return 2;
 HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName),finish=OpenEventW(SYNCHRONIZE,FALSE,exitName);
 if(!ready || !finish || !SetEvent(ready) || WaitForSingleObject(finish,10000)!=WAIT_OBJECT_0) return 3;
 /* Deliberately rely on process teardown rather than Release: a dead owner
    cannot leave a persistent lock file or stale reader count. */
 return 0;
}
static int processLeaseCheck(const char* path,const char* cache,uint64_t future)
{
 wchar_t exe[32768],readyName[160],exitName[160]; DWORD n=GetModuleFileNameW(NULL,exe,32768); if(!n || n>=32768) return 0;
 swprintf(readyName,160,L"Local\\XideStageReady-%lu-%llu",(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 swprintf(exitName,160,L"Local\\XideStageExit-%lu-%llu",(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 HANDLE ready=CreateEventW(NULL,TRUE,FALSE,readyName),finish=CreateEventW(NULL,TRUE,FALSE,exitName); wchar_t* wide=xidePathNativeWide(path);
 size_t capacity=wcslen(exe)+(wide ? wcslen(wide) : 0)+wcslen(readyName)+wcslen(exitName)+40; wchar_t* command=malloc(capacity*sizeof(*command));
 PROCESS_INFORMATION process={0}; STARTUPINFOW startup={.cb=sizeof(startup)}; int okay=0;
 if(!ready || !finish || !wide || !command) goto done;
 swprintf(command,capacity,L"\"%ls\" --lease \"%ls\" \"%ls\" \"%ls\"",exe,wide,readyName,exitName);
 if(!CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) goto done;
 xide_task_stage_stats stats={0}; xide_model_error error={0};
 okay=WaitForSingleObject(ready,5000)==WAIT_OBJECT_0 && xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(path);
 SetEvent(finish); DWORD result=0; okay=WaitForSingleObject(process.hProcess,5000)==WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess,&result) && !result && okay;
done:
 if(process.hThread) CloseHandle(process.hThread);
 if(process.hProcess) CloseHandle(process.hProcess);
 if(ready) CloseHandle(ready);
 if(finish) CloseHandle(finish);
 free(wide); free(command); return okay;
}
int main(int argc,char** argv)
{
 int child=readerChild(argc,argv); if(child>=0) return child;
 int checks=0,failed=0; xide_model_error error={0}; xide_task_stage *stage=NULL,*other=NULL,*reference=NULL;
 char *root=NULL,*cache=NULL,*first=NULL,*marker=NULL,*legacy=NULL,*legacyBody=NULL,*invalid=NULL,*deep=NULL,*deepStage=NULL,*deepMarker=NULL,*owned=NULL,*body=NULL,*unknown=NULL;
 unsigned char* bytes=NULL; size_t size=0; xide_task_stage_stats stats={0}; uint64_t future=nowTicks()+XIDE_TASK_STAGE_GRACE_TICKS+UINT64_C(24)*60*60*10000000;
 CHECK(argc==2); root=xidePathResolveLexical(NULL,argv[1]); cache=root ? xidePathResolveLexical(root,".xlang-ide-build") : NULL; CHECK(root && cache);
 first=xidePathResolveLexical(cache,"01234567-89ab-cdef-0123-456789abcdef"); marker=first ? xidePathResolveLexical(first,".xide-stage") : NULL;
 CHECK(first && marker && (stage=xideTaskStageBegin(first,&error)) && leaseUnavailable(marker));
 other=xideTaskStageBegin(first,&error); CHECK(!other && leaseUnavailable(marker));
 CHECK(xideTaskStageComplete(stage,&error));
 CHECK((reference=xideTaskStageProtectPath(first,&error)) && !error.code);
 xideTaskStageRelease(stage); stage=NULL;
 CHECK(!xideTaskStageComplete(reference,&error));
 bytes=xideIoReadAllLimit(marker,2048,&size); CHECK(bytes && size==848 && !memcmp(bytes,"xlang-ide-stage1",16) && bytes[16]==1 && bytes[20]==2 && !memcmp(bytes+24,"01234567-89ab-cdef-0123-456789abcdef",36)); xrtFree(bytes); bytes=NULL;
 CHECK(!xideTaskStageBegin(first,&error));
 legacy=xidePathResolveLexical(cache,"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"); legacyBody=legacy ? xidePathResolveLexical(legacy,"user.txt") : NULL;
 const char text[]="user-owned legacy cache fixture";
 CHECK(legacy && legacyBody && xideIoCreateDirectories(legacy) && xideIoWriteAtomic(legacyBody,(xbytesview){(const unsigned char*)text,sizeof(text)-1}));
 CHECK(!xideTaskStageBegin(legacy,&error)); bytes=xideIoReadAllLimit(legacyBody,256,&size); CHECK(bytes && size==sizeof(text)-1 && !memcmp(bytes,text,size)); xrtFree(bytes); bytes=NULL;
 invalid=xidePathResolveLexical(root,"foreign/not-a-guid"); CHECK(invalid && !xideTaskStageBegin(invalid,&error) && !xideIoExists(invalid));
 deep=xidePathResolveLexical(root,"深目录 😀"); CHECK(deep);
 for(int i=0;i<16;++i) { char* next=xidePathResolveLexical(deep,"中文任务目录0123456789"); CHECK(next); free(deep); deep=next; }
 deepStage=xidePathResolveLexical(deep,".xlang-ide-build/11234567-89ab-cdef-0123-456789abcdef"); deepMarker=deepStage ? xidePathResolveLexical(deepStage,".xide-stage") : NULL;
 CHECK(deepStage && deepMarker && (stage=xideTaskStageBegin(deepStage,&error)) && leaseUnavailable(deepMarker) && xideTaskStageComplete(stage,&error));
 char* moved=xidePathResolveLexical(root,"移动完成的项目 😀"); CHECK(moved && moveFixtureDirectory(deep,moved)); free(deep); deep=moved;
 free(deepStage); free(deepMarker); deepStage=xidePathResolveLexical(deep,".xlang-ide-build/11234567-89ab-cdef-0123-456789abcdef"); deepMarker=deepStage ? xidePathResolveLexical(deepStage,".xide-stage") : NULL;
 /* The lease survives a parent move; cache/directory physical identities remain
    valid even though every stored display path has changed. */
 CHECK(deepStage && deepMarker && (other=xideTaskStageProtectPath(deepStage,&error)) && !error.code); xideTaskStageRelease(stage); stage=NULL;
 char* deepCache=xidePathDirectory(deepStage); CHECK(deepCache);
 int keptDeep=xideTaskStagePrune(deepCache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(deepMarker); CHECK(keptDeep);
 xideTaskStageRelease(other); other=NULL;
 bytes=xideIoReadAllLimit(deepMarker,2048,&size); CHECK(bytes && size==848 && bytes[20]==2); xrtFree(bytes); bytes=NULL;
 keptDeep=xideTaskStagePrune(deepCache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && !xideIoExists(deepStage); free(deepCache); CHECK(keptDeep);
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && stats.completed==1 && xideIoExists(marker));
 xideTaskStageRelease(reference); reference=NULL;
 CHECK(!xideTaskStageProtectPath(legacyBody,&error) && !error.code);
 char* foreignMarker=xidePathResolveLexical(legacy,".xide-stage"); CHECK(foreignMarker && xideIoWriteAtomic(foreignMarker,(xbytesview){(const unsigned char*)text,sizeof(text)-1}));
 CHECK(!xideTaskStageProtectPath(legacyBody,&error) && !error.code); free(foreignMarker);
 CHECK(xideTaskStagePrune(cache,0,nowTicks(),NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(marker));
 const char* protectFirst[]={first}; xide_strings protected={.items=(char**)protectFirst,.count=1};
 CHECK(xideTaskStagePrune(cache,0,future,&protected,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(marker));
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && stats.deletedFiles==1 && !xideIoExists(first) && xideIoExists(legacyBody));
 owned=xidePathResolveLexical(cache,"22222222-3333-4444-5555-666666666666"); body=owned ? xidePathResolveLexical(owned,"request.xlp") : NULL; unknown=owned ? xidePathResolveLexical(owned,"user.txt") : NULL;
 CHECK(owned && body && unknown && (stage=xideTaskStageBegin(owned,&error)));
 unsigned char original[32]; memset(original,'A',sizeof(original)); CHECK(xideIoWriteAtomic(body,(xbytesview){original,sizeof(original)}) && xideTaskStageComplete(stage,&error));
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body)); xideTaskStageRelease(stage); stage=NULL;
 CHECK((reference=xideTaskStageProtectPath(body,&error)) && !error.code);
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body));
 xideTaskStageRelease(reference); reference=NULL;
 wchar_t* bodyWide=xidePathNativeWide(body); CHECK(bodyWide);
 HANDLE busy=CreateFileW(bodyWide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(busy!=INVALID_HANDLE_VALUE);
 int kept=xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body); CloseHandle(busy); CHECK(kept);
 CHECK(SetFileAttributesW(bodyWide,FILE_ATTRIBUTE_READONLY));
 kept=xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body);
 CHECK(SetFileAttributesW(bodyWide,FILE_ATTRIBUTE_NORMAL) && kept);
 char* alias=xidePathResolveLexical(root,"protected-hardlink.txt"); wchar_t* aliasWide=alias ? xidePathNativeWide(alias) : NULL; CHECK(aliasWide && CreateHardLinkW(aliasWide,bodyWide,NULL));
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body) && xideIoExists(alias));
 /* Remove only the freshly created hardlink, whose two identities were checked. */
 HANDLE linked=CreateFileW(aliasWide,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); BY_HANDLE_FILE_INFORMATION linkInfo={0};
 kept=linked!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(linked,&linkInfo) && linkInfo.nNumberOfLinks==2; if(linked!=INVALID_HANDLE_VALUE) CloseHandle(linked);
 CHECK(kept && DeleteFileW(aliasWide)); free(alias); free(aliasWide); free(bodyWide);
 const char* protectBody[]={body}; protected.items=(char**)protectBody;
 CHECK(xideTaskStagePrune(cache,0,future,&protected,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body));
 CHECK(editPreservingMetadata(body,'B') && xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body));
 CHECK(editPreservingMetadata(body,'A'));
 CHECK(xideIoWriteAtomic(unknown,(xbytesview){(const unsigned char*)text,sizeof(text)-1}) && xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body) && xideIoExists(unknown));
 /* Move only the named fixture file to another verified fixture path; unknown
    bytes stay preserved and the stage can then test the known inventory. */
 char* retained=xidePathResolveLexical(root,"retained-user.txt"); CHECK(retained && xideIoMove(unknown,retained,0)); free(retained);
 CHECK(!xideTaskStagePrune(cache,0,future,NULL,cancelNow,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body));
 CHECK(xideTaskStagePrune(cache,1,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages && xideIoExists(body));
 CHECK(processLeaseCheck(body,cache,future));
 CHECK(xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages==1 && stats.deletedFiles==2 && stats.releasedBytes==880 && !xideIoExists(owned) && xideIoExists(legacyBody));
 printf("Task stage ownership checks: %d passed; identity and digest inventory, live/history/shared-reference gates, grace/ranking, busy/readonly/hardlink preservation, unknown/legacy files and deep Unicode stage\n",checks);
done:
 xideTaskStageRelease(stage); xideTaskStageRelease(other); xideTaskStageRelease(reference); xrtFree(bytes);
 char* paths[]={root,cache,first,marker,legacy,legacyBody,invalid,deep,deepStage,deepMarker,owned,body,unknown}; for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) free(paths[i]); return failed;
}
