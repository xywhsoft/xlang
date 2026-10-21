#define _WIN32_WINNT 0x0601
#include "platform/file_watch.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"file_watch_test:%d: %s; %s; Win32=%lu\n",__LINE__,#v,error.message,GetLastError()); failed=1; goto cleanup; } } while(0)
static volatile LONG wakes;
static void woke(void* unused) { (void)unused; InterlockedIncrement(&wakes); }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* p=n ? malloc((size_t)n*sizeof(*p)) : NULL; if(p) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,p,n); return p; }
static int writeText(const char* path,const char* s) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int removeFile(const char* path) { wchar_t* p=wide(path); int okay=p && DeleteFileW(p); free(p); return okay; }
static int waitReady(xide_file_watch* w,uint64_t epoch)
{ uint64_t due=GetTickCount64()+12000; do { xide_watch_stats s; xideFileWatchStats(w,&s); if(s.readyEpoch==epoch) return 1; Sleep(10); } while(GetTickCount64()<due); return 0; }
static void drain(xide_file_watch* w) { xide_watch_event e={0}; while(xideFileWatchTake(w,&e)) xideFileWatchEventClear(&e); }
static int observed(xide_file_watch* w,uint64_t epoch,uint64_t token,int state,const char* text,xide_watch_event* result)
{
    uint64_t due=GetTickCount64()+12000; xide_watch_event e={0};
    do {
        while(xideFileWatchTake(w,&e)) {
            if(e.epoch==epoch && e.token==token && e.state==state && (!text || (e.length==strlen(text) && e.bytes && !memcmp(e.bytes,text,e.length)))) { *result=e; return 1; }
            xideFileWatchEventClear(&e);
        }
        Sleep(10);
    } while(GetTickCount64()<due); return 0;
}
static int childWrite(const char* path)
{
    wchar_t executable[4096]; if(!GetModuleFileNameW(NULL,executable,4096)) return 0;
    wchar_t* target=wide(path); size_t n=wcslen(executable)+(target ? wcslen(target) : 0)+40;
    wchar_t* command=calloc(n,sizeof(*command)); PROCESS_INFORMATION process={0}; STARTUPINFOW startup={.cb=sizeof(startup)};
    int okay=target && command && swprintf(command,n,L"\"%ls\" --child-write \"%ls\"",executable,target)>0 && CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process);
    if(okay) { DWORD code=1; okay=WaitForSingleObject(process.hProcess,15000)==WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess,&code) && code==0; if(!okay) TerminateProcess(process.hProcess,1); CloseHandle(process.hThread); CloseHandle(process.hProcess); }
    free(target); free(command); return okay;
}
int main(int argc,char** argv)
{
    if(argc==3 && !strcmp(argv[1],"--child-write")) {
        int n=0; wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&n); char target[32768];
        int okay=args && n==3 && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,args[2],-1,target,sizeof(target),NULL,NULL) && writeText(target,"// child 中文 😀\n");
        if(args) LocalFree(args);
        return okay ? 0 : 1;
    }
    int failed=0; xide_model_error error={0}; xide_file_watch* watch=NULL; xide_watch_event event={0}; xide_watch_stats stats={0};
    char *root=NULL,*source=NULL,*alias=NULL,*missing=NULL,*oversize=NULL,*path=NULL,*dir=NULL; wchar_t *sourceWide=NULL,*aliasWide=NULL; HANDLE locked=INVALID_HANDLE_VALUE;
    char* paths[72]={0}; xide_watch_target targets[72]={0}; unsigned char* big=NULL; xide_watch_target* many=NULL;
    CHECK(argc==2); char* id=xideCreateId("watch-test"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    source=xidePathResolve(root,"中文 命令.xl"); alias=xidePathResolve(root,"硬链接.c"); missing=xidePathResolve(root,"新建 目录/更深/file.xl"); oversize=xidePathResolve(root,"large.c"); CHECK(source && alias && missing && oversize && writeText(source,"first\n"));
    watch=xideFileWatchCreate(woke,NULL,&error); CHECK(watch);
    targets[0]=(xide_watch_target){1,source,1024}; targets[1]=(xide_watch_target){2,missing,1024}; targets[2]=(xide_watch_target){3,oversize,2}; targets[3]=(xide_watch_target){4,source,0};
    uint64_t epoch=xideFileWatchSetTargets(watch,targets,4,&error); CHECK(epoch && waitReady(watch,epoch));
    CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"first\n",&event)); CHECK(event.path && xidePathEqual(event.path,source) && event.fileId && event.volume && event.sequence && event.hash); xideFileWatchEventClear(&event);
    xideFileWatchRescan(watch); CHECK(observed(watch,epoch,2,XIDE_WATCH_MISSING,NULL,&event)); xideFileWatchEventClear(&event); drain(watch);
    xideFileWatchStats(watch,&stats); uint64_t notifications=stats.notifications;
    CHECK(childWrite(source)); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"// child 中文 😀\n",&event)); xideFileWatchEventClear(&event); xideFileWatchStats(watch,&stats); CHECK(stats.notifications>notifications);
    CHECK(writeText(source,"111111\n")); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"111111\n",&event)); xideFileWatchEventClear(&event);
    CHECK(writeText(source,"222222\n")); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"222222\n",&event)); xideFileWatchEventClear(&event);
    sourceWide=wide(source); aliasWide=wide(alias); CHECK(sourceWide && aliasWide && CreateHardLinkW(aliasWide,sourceWide,NULL));
    targets[4]=(xide_watch_target){5,alias,1024}; epoch=xideFileWatchSetTargets(watch,targets,5,&error); CHECK(epoch && waitReady(watch,epoch));
    CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"222222\n",&event)); uint64_t identity=event.fileId; xideFileWatchEventClear(&event);
    xideFileWatchRescan(watch); CHECK(observed(watch,epoch,5,XIDE_WATCH_READY,"222222\n",&event) && event.fileId==identity); xideFileWatchEventClear(&event);
    HANDLE writer=CreateFileW(aliasWide,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); CHECK(writer!=INVALID_HANDLE_VALUE); DWORD written=0; int wrote=WriteFile(writer,"hardlink\n",9,&written,NULL) && written==9 && SetEndOfFile(writer) && FlushFileBuffers(writer); CloseHandle(writer); CHECK(wrote);
    CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"hardlink\n",&event)); xideFileWatchEventClear(&event);
    CHECK(writeText(source,"replacement\n")); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"replacement\n",&event) && event.fileId!=identity); xideFileWatchEventClear(&event);
    CHECK(removeFile(source)); CHECK(observed(watch,epoch,1,XIDE_WATCH_MISSING,NULL,&event)); xideFileWatchEventClear(&event);
    CHECK(writeText(source,"recreated\n")); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"recreated\n",&event)); xideFileWatchEventClear(&event);
    locked=CreateFileW(sourceWide,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL); CHECK(locked!=INVALID_HANDLE_VALUE); xideFileWatchRescan(watch);
    CHECK(observed(watch,epoch,1,XIDE_WATCH_UNREADABLE,NULL,&event) && event.error==ERROR_SHARING_VIOLATION); xideFileWatchEventClear(&event); CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    xideFileWatchRescan(watch); CHECK(observed(watch,epoch,1,XIDE_WATCH_READY,"recreated\n",&event)); xideFileWatchEventClear(&event);
    dir=xidePathDirectory(missing); CHECK(dir && xrtDirCreateAll(dir) && writeText(missing,"new parent\n")); free(dir); dir=NULL;
    CHECK(observed(watch,epoch,2,XIDE_WATCH_READY,"new parent\n",&event)); xideFileWatchEventClear(&event);
    CHECK(writeText(oversize,"abc")); CHECK(observed(watch,epoch,3,XIDE_WATCH_TOO_LARGE,NULL,&event) && event.size==3 && !event.bytes); xideFileWatchEventClear(&event);
    CHECK(writeText(oversize,"")); CHECK(observed(watch,epoch,3,XIDE_WATCH_READY,"",&event)); xideFileWatchEventClear(&event);
    /* No WaitForMultipleObjects limit: each target has a distinct native directory. */
    for(size_t i=0;i<72;++i) { char relative[80]; snprintf(relative,sizeof(relative),"folder-%zu/file.c",i); paths[i]=xidePathResolve(root,relative); dir=paths[i] ? xidePathDirectory(paths[i]) : NULL; CHECK(paths[i] && dir && xrtDirCreateAll(dir) && writeText(paths[i],"initial\n")); free(dir); dir=NULL; targets[i]=(xide_watch_target){100+i,paths[i],1024}; }
    epoch=xideFileWatchSetTargets(watch,targets,72,&error); CHECK(epoch && waitReady(watch,epoch)); xideFileWatchStats(watch,&stats); CHECK(stats.directories==72); drain(watch);
    notifications=stats.notifications; CHECK(writeText(paths[71],"directory 72\n")); CHECK(observed(watch,epoch,171,XIDE_WATCH_READY,"directory 72\n",&event)); xideFileWatchEventClear(&event); xideFileWatchStats(watch,&stats); CHECK(stats.notifications>notifications);
    targets[1].token=targets[0].token; CHECK(!xideFileWatchSetTargets(watch,targets,2,&error) && error.code==XIDE_MODEL_INVALID); targets[1].token=101;
    targets[0].limit=16u*1024u*1024u+1; CHECK(!xideFileWatchSetTargets(watch,targets,1,&error)); targets[0].limit=1024;
    CHECK(!xideFileWatchSetTargets(watch,NULL,1,&error));
    /* A replacement epoch discards even already queued old-identity snapshots. */
    xideFileWatchRescan(watch); Sleep(50); targets[0]=(xide_watch_target){777,source,1024}; epoch=xideFileWatchSetTargets(watch,targets,1,&error); CHECK(epoch && waitReady(watch,epoch));
    while(xideFileWatchTake(watch,&event)) { CHECK(event.epoch==epoch && event.token==777); xideFileWatchEventClear(&event); }
    CHECK(writeText(source,"coalesced one\n") && writeText(source,"coalesced two\n")); Sleep(200); xideFileWatchStats(watch,&stats); CHECK(stats.queued<=1 && stats.queuedBytes<=1024);
    CHECK(observed(watch,epoch,777,XIDE_WATCH_READY,"coalesced two\n",&event)); xideFileWatchEventClear(&event);
    /* Bound metadata event count; dropped targets retry on the non-forced sweep. */
    many=calloc(4100,sizeof(*many)); CHECK(many); for(size_t i=0;i<4100;++i) many[i]=(xide_watch_target){10000+i,source,0};
    epoch=xideFileWatchSetTargets(watch,many,4100,&error); CHECK(epoch && waitReady(watch,epoch)); xideFileWatchStats(watch,&stats); CHECK(stats.queued==4096 && stats.dropped>=4 && !stats.queuedBytes); drain(watch);
    CHECK(observed(watch,epoch,14099,XIDE_WATCH_READY,NULL,&event)); xideFileWatchEventClear(&event); free(many); many=NULL;
    /* Body queue caps memory at 128 MiB even for independently owned snapshots. */
    size_t bigSize=16u*1024u*1024u; big=malloc(bigSize); CHECK(big); memset(big,'a',bigSize); CHECK(xrtFileWriteAtomic(oversize,(xbytesview){big,bigSize})); free(big); big=NULL;
    for(size_t i=0;i<9;++i) targets[i]=(xide_watch_target){20000+i,oversize,bigSize};
    epoch=xideFileWatchSetTargets(watch,targets,9,&error); CHECK(epoch && waitReady(watch,epoch)); xideFileWatchStats(watch,&stats); CHECK(stats.queuedBytes==128u*1024u*1024u && stats.queued==8 && stats.dropped>=5); drain(watch);
    CHECK(observed(watch,epoch,20008,XIDE_WATCH_READY,NULL,&event) && event.length==bigSize); xideFileWatchEventClear(&event);
    /* Switching subscriptions interrupts a long obsolete body scan. */
    many=calloc(1024,sizeof(*many)); CHECK(many); for(size_t i=0;i<1024;++i) many[i]=(xide_watch_target){30000+i,oversize,bigSize};
    uint64_t obsolete=xideFileWatchSetTargets(watch,many,1024,&error); CHECK(obsolete); Sleep(30);
    targets[0]=(xide_watch_target){90000,source,1024}; epoch=xideFileWatchSetTargets(watch,targets,1,&error); CHECK(epoch && waitReady(watch,epoch));
    while(xideFileWatchTake(watch,&event)) { CHECK(event.epoch==epoch && event.token==90000); xideFileWatchEventClear(&event); } free(many); many=NULL;
    CHECK(wakes>0); epoch=xideFileWatchSetTargets(watch,NULL,0,&error); CHECK(epoch && waitReady(watch,epoch)); xideFileWatchStats(watch,&stats); CHECK(!stats.queued && !stats.directories && !stats.targets);
    xideFileWatchDestroy(watch); watch=NULL;
    for(int i=0;i<12;++i) { watch=xideFileWatchCreate(NULL,NULL,&error); CHECK(watch); targets[0]=(xide_watch_target){1,source,1024}; CHECK(xideFileWatchSetTargets(watch,targets,1,&error)); xideFileWatchDestroy(watch); watch=NULL; }
    puts("file_watch_test passed: real IOCP notifications and another process, UTF-8 paths/body, atomic/same-size/hardlink changes, missing/recreated/locked/oversize/empty files, missing parents, 72 directories, epochs, coalescing, 4096-event and 128-MiB limits with retries, empty subscriptions and cancellation teardown");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideFileWatchEventClear(&event); xideFileWatchDestroy(watch); free(root); free(source); free(alias); free(missing); free(oversize); free(path); free(dir); free(sourceWide); free(aliasWide); free(big); free(many); for(size_t i=0;i<72;++i) free(paths[i]); return failed;
}
