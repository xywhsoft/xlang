#include "platform/file_watch.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static volatile LONG checks;
#define CHECK(v) do{InterlockedIncrement(&checks);if(!(v)){fprintf(stderr,"watch-lease:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static HANDLE arrived,releaseRead;static volatile LONG blockRead,trackOwner,workerFrees;
static DWORD owner,volume,indexLow,indexHigh;static size_t ownerFrees,ownerAllocations;
static void* volatile savedBody;static volatile LONG captureBody,bodyFreedByWorker;
extern BOOL (WINAPI* __real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
void __real_free(void*);void* __real_malloc(size_t);
void __wrap_free(void* p)
{if(p==InterlockedCompareExchangePointer(&savedBody,NULL,NULL)&&p)InterlockedExchange(&bodyFreedByWorker,GetCurrentThreadId()!=owner);if(InterlockedCompareExchange(&trackOwner,0,0)){if(GetCurrentThreadId()==owner)++ownerFrees;else if(p)InterlockedIncrement(&workerFrees);}__real_free(p);}
void* __wrap_malloc(size_t n)
{if(InterlockedCompareExchange(&trackOwner,0,0)&&GetCurrentThreadId()==owner)++ownerAllocations;return __real_malloc(n);}
static BOOL WINAPI readProbe(HANDLE h,LPVOID bytes,DWORD amount,LPDWORD got,LPOVERLAPPED at)
{
 BY_HANDLE_FILE_INFORMATION i={0};
 if(amount&&GetFileInformationByHandle(h,&i)&&i.dwVolumeSerialNumber==volume&&i.nFileIndexLow==indexLow&&i.nFileIndexHigh==indexHigh){
  if(InterlockedCompareExchange(&captureBody,0,0))InterlockedCompareExchangePointer(&savedBody,bytes,NULL);
  if(InterlockedCompareExchange(&blockRead,0,1)==1){CHECK(SetEvent(arrived));CHECK(WaitForSingleObject(releaseRead,30000)==WAIT_OBJECT_0);}
 }
 return __real___imp_ReadFile(h,bytes,amount,got,at);
}
BOOL (WINAPI* __wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=readProbe;
static void writeText(const char* path,const char* text)
{CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}));}
static void waitReady(xide_file_watch* w,uint64_t epoch)
{uint64_t end=GetTickCount64()+12000;do{xide_watch_stats s;xideFileWatchStats(w,&s);if(s.readyEpoch==epoch)return;Sleep(5);}while(GetTickCount64()<end);CHECK(0);}
static void waitHeld(xide_file_watch* w,uint64_t token)
{uint64_t end=GetTickCount64()+12000;do{if(xideFileWatchSuspended(w,token))return;Sleep(5);}while(GetTickCount64()<end);CHECK(0);}
static void verifyFresh(xide_file_watch* w,uint64_t epoch,uint64_t began,const char* expected)
{
 int body=0,gap=0;uint64_t end=GetTickCount64()+12000;xide_watch_event e={0};
 do{while(xideFileWatchTake(w,&e)){CHECK(e.epoch==epoch&&e.observedAt>=began);if(e.token){CHECK(e.token==1&&e.state==XIDE_WATCH_READY&&e.length==strlen(expected)&&!memcmp(e.bytes,expected,e.length));body=1;}else{CHECK(e.state==XIDE_WATCH_SOURCE_RESCAN);gap=1;}xideFileWatchEventClear(&e);}if(body&&gap)return;Sleep(5);}while(GetTickCount64()<end);CHECK(0);
}
int main(int argc,char** argv)
{
 CHECK(argc==2&&xrtDirCreateAll(argv[1]));owner=GetCurrentThreadId();char* source=xidePathResolveLexical(argv[1],"lease-中文.xl");CHECK(source);writeText(source,"before\n");writeText(source,"before\n");
 wchar_t* name=xidePathNativeWide(source);HANDLE h=CreateFileW(name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(name);BY_HANDLE_FILE_INFORMATION id;CHECK(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(h,&id)&&CloseHandle(h));volume=id.dwVolumeSerialNumber;indexLow=id.nFileIndexLow;indexHigh=id.nFileIndexHigh;
 arrived=CreateEventW(NULL,TRUE,FALSE,NULL);releaseRead=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(arrived&&releaseRead);DWORD baseline;CHECK(GetProcessHandleCount(GetCurrentProcess(),&baseline));xide_model_error e={0};xide_file_watch* watch=xideFileWatchCreate(NULL,NULL,&e);CHECK(watch);
 InterlockedExchange(&captureBody,1);xide_watch_target target={1,source,1024};uint64_t epoch=xideFileWatchSetTargetsWithSources(watch,&target,1,1,&e);CHECK(epoch);waitReady(watch,epoch);xide_watch_stats s;xideFileWatchStats(watch,&s);CHECK(s.queued&&s.queuedBytes==7&&InterlockedCompareExchangePointer(&savedBody,NULL,NULL));InterlockedExchange(&captureBody,0);
 InterlockedExchange(&blockRead,1);xideFileWatchRescan(watch);CHECK(WaitForSingleObject(arrived,12000)==WAIT_OBJECT_0);
 ownerAllocations=ownerFrees=0;InterlockedExchange(&workerFrees,0);InterlockedExchange(&trackOwner,1);uint64_t token=xideFileWatchSuspend(watch,&e);CHECK(token&&ownerFrees==0&&ownerAllocations==0);
 CHECK(!xideFileWatchSuspended(watch,token));xide_watch_event event={0};CHECK(!xideFileWatchTake(watch,&event));CHECK(!xideFileWatchSuspend(watch,&e));CHECK(!xideFileWatchSetTargets(watch,NULL,0,&e));CHECK(!xideFileWatchResume(watch,token,&target,1,1,&e));CHECK(!xideFileWatchResume(watch,token+1,&target,1,1,&e));
 CHECK(SetEvent(releaseRead));waitHeld(watch,token);InterlockedExchange(&trackOwner,0);CHECK(InterlockedCompareExchange(&bodyFreedByWorker,0,0)&&InterlockedCompareExchange(&workerFrees,0,0)>0);InterlockedExchangePointer(&savedBody,NULL);xideFileWatchStats(watch,&s);CHECK(s.epoch==token&&s.readyEpoch==token&&!s.targets&&!s.directories&&!s.queued&&!s.queuedBytes);CHECK(!xideFileWatchTake(watch,&event));
 writeText(source,"committed\n");char* candidate=xidePathResolveLexical(argv[1],"created-while-paused.xl");CHECK(candidate);writeText(candidate,"candidate\n");xideFileWatchRescan(watch);Sleep(30);CHECK(xideFileWatchSuspended(watch,token)&&!xideFileWatchTake(watch,&event));
 target.limit=16u*1024u*1024u+1;CHECK(!xideFileWatchResume(watch,token,&target,1,1,&e)&&xideFileWatchSuspended(watch,token));target.limit=1024;
 uint64_t began=GetTickCount64();epoch=xideFileWatchResume(watch,token,&target,1,1,&e);CHECK(epoch>token&&!xideFileWatchSuspended(watch,token));waitReady(watch,epoch);verifyFresh(watch,epoch,began,"committed\n");CHECK(!xideFileWatchResume(watch,token,&target,1,1,&e));
 /* A second blocked read proves pending metadata is also retired off-owner. */
 name=xidePathNativeWide(source);h=CreateFileW(name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(name);CHECK(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(h,&id)&&CloseHandle(h));volume=id.dwVolumeSerialNumber;indexLow=id.nFileIndexLow;indexHigh=id.nFileIndexHigh;CHECK(ResetEvent(arrived)&&ResetEvent(releaseRead));InterlockedExchange(&blockRead,1);xideFileWatchRescan(watch);CHECK(WaitForSingleObject(arrived,12000)==WAIT_OBJECT_0);
 xide_watch_target many[128];for(size_t i=0;i<128;++i)many[i]=(xide_watch_target){100+i,source,1024};CHECK(xideFileWatchSetTargets(watch,many,128,&e));ownerAllocations=ownerFrees=0;InterlockedExchange(&workerFrees,0);InterlockedExchange(&trackOwner,1);
 uint64_t next=xideFileWatchSuspend(watch,&e);CHECK(next>epoch&&!ownerFrees&&!ownerAllocations&&!xideFileWatchSuspended(watch,next));CHECK(SetEvent(releaseRead));waitHeld(watch,next);InterlockedExchange(&trackOwner,0);CHECK(InterlockedCompareExchange(&workerFrees,0,0)>=128);CHECK(!xideFileWatchSuspended(watch,token));xideFileWatchDestroy(watch);watch=NULL;
 for(unsigned i=0;i<12;++i){watch=xideFileWatchCreate(NULL,NULL,&e);CHECK(watch&&xideFileWatchSetTargetsWithSources(watch,&target,1,1,&e));CHECK(xideFileWatchSuspend(watch,&e));xideFileWatchDestroy(watch);}
 DWORD after;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==baseline);CHECK(CloseHandle(arrived)&&CloseHandle(releaseRead));free(candidate);free(source);
 printf("file_watch_lease_test passed: %ld checks; actual blocked worker read, nonblocking owner request without frees/allocations, deferred metadata retirement, exact acknowledged tokens, fresh body/source invalidation, failed resume and handle balance\n",checks);return 0;
}
