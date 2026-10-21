#include "platform/recovery_backup.h"
#include "services/recovery.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"recovery_backup_test:%d: %s; %s; Win32=%lu\n",__LINE__,#v,error.message,GetLastError()); failed=1; goto cleanup; } } while(0)
static int writeRaw(const char* p,const unsigned char* raw,size_t count)
{ char* native=xidePathNativeUtf8(p); int okay=native && xrtFileWriteAtomic(native,(xbytesview){raw,count}); free(native); return okay; }
static int writeText(const char* p,const char* s) { return writeRaw(p,(const unsigned char*)s,strlen(s)); }
static int same(const char* p,const unsigned char* expected,size_t count)
{ char* native=xidePathNativeUtf8(p); size_t n=0; unsigned char* raw=native ? xrtFileReadAllLimit(native,16*1024*1024,&n) : NULL; free(native); int equal=raw && n==count && !memcmp(raw,expected,count); xrtFree(raw); return equal; }
static uint64_t now(void) { FILETIME t; GetSystemTimeAsFileTime(&t); return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int age(const char* p)
{
    wchar_t* w=xidePathNativeWide(p); if(!w) return 0;
    HANDLE h=CreateFileW(w,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    free(w);
    uint64_t old=now()-UINT64_C(9)*24*60*60*10000000; FILETIME t={(DWORD)old,(DWORD)(old>>32)};
    int okay=h!=INVALID_HANDLE_VALUE && SetFileTime(h,&t,NULL,&t); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); return okay;
}
int main(int argc,char** argv)
{
    int failed=0; xide_model_error error={0}; xide_recovery_guard guard={0}; xide_recovery_backup backup={0}; xide_recovery_list list={0}; xide_text_file preview={0}; xide_recovery_stats stats={0};
    char *root=NULL,*metadata=NULL,*layout=NULL,*previous=NULL,*body=NULL,*base=NULL,*invalid=NULL,*foreign=NULL,*archive=NULL,*copyPath=NULL; unsigned char* large=NULL; HANDLE locked=INVALID_HANDLE_VALUE; volatile LONG cancel=0; xide_recovery_service* service=NULL;
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--deep"))); char* id=xideCreateId("backup-test"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id); CHECK(root);
    if(argc==3) {
        for(int i=0;i<32;++i) { wchar_t* w=xidePathNativeWide(root); size_t units=w ? wcslen(w) : 0; free(w); CHECK(units);
            if(units>=400) break;
            char* next=xidePathResolve(root,"中文😀-long-recovery-segment"); CHECK(next); free(root); root=next;
        }
        wchar_t* w=xidePathNativeWide(root); CHECK(w && wcslen(w)>=400); free(w);
        CHECK(!strstr(root,"//?/") && strchr(root,'/') && strstr(root,"中文😀"));
    }
    CHECK(xideRecoveryLock(&guard,root,&error));
    metadata=xideRecoveryPath(root,"session.json",&error); layout=xideRecoveryPath(root,"layout.json",&error); previous=xideRecoveryPath(root,"previous-session.json",&error);
    body=xideRecoveryPath(root,"recovery/doc-11111111-2222-3333-4444-555555555555-1234567890123456.text",&error); base=xideRecoveryPath(root,"recovery/doc-11111111-2222-3333-4444-555555555555-1234567890123456.base",&error);
    invalid=xideRecoveryPath(root,"recovery/invalid.text",&error); foreign=xideRecoveryPath(root,"recovery/user.txt",&error); CHECK(metadata && layout && previous && body && base && invalid && foreign);
    size_t size=8*1024*1024+17; large=malloc(size); CHECK(large); memset(large,0xfe,size); memcpy(large,"broken state",12);
    const unsigned char rawLayout[]={0xff,0,0x41,0x42}; const char* code="int main(void) { return 42; } // 中文😀\n";
    CHECK(writeRaw(metadata,large,size) && writeRaw(layout,rawLayout,sizeof(rawLayout)) && writeText(previous,"{\"format\":\"future-99\"}"));
    CHECK(writeText(body,code) && writeText(base,"\xef\xbb\xbf// baseline\r\n") && writeRaw(invalid,rawLayout,sizeof(rawLayout)) && writeText(foreign,"foreign retained"));
    CHECK(xideRecoveryListRead(&guard,&list,&cancel,&error) && list.count==2); CHECK(xideRecoveryRead(&guard,list.items[0].reference,&preview,&error) || xideRecoveryRead(&guard,list.items[1].reference,&preview,&error));
    CHECK(!xideRecoveryRead(&guard,"recovery/../source.c",&preview,&error)); xideRecoveryUnlock(&guard); xideRecoveryListClear(&list);
    CHECK(xideRecoveryBackupCreate(root,&cancel,&backup,&error) && backup.retired==7 && backup.files==7 && !xidePathExists(metadata) && !xidePathExists(layout) && !xidePathExists(previous));
    archive=xidePathResolve(NULL,backup.directory); copyPath=xidePathResolve(archive,"session.json"); CHECK(archive && copyPath && same(copyPath,large,size)); free(copyPath); copyPath=xidePathResolve(archive,"layout.json"); CHECK(copyPath && same(copyPath,rawLayout,sizeof(rawLayout))); free(copyPath); copyPath=NULL;
    CHECK(xideRecoveryBackupsCheck(&backup.guard,&error) && xideRecoveryListRead(&backup.guard,&list,&cancel,&error) && list.count==4);
    int archived=-1; for(size_t i=0;i<list.count;++i) if(list.items[i].archived && strstr(list.items[i].reference,"1234567890123456.text")) archived=(int)i; CHECK(archived>=0 && xideRecoveryRead(&backup.guard,list.items[archived].reference,&preview,&error) && !strcmp(preview.text,code));
    CHECK(xideRecoveryBackupUndo(&backup,&error) && !backup.retired && same(metadata,large,size) && same(layout,rawLayout,sizeof(rawLayout)));
    xideRecoveryBackupClear(&backup); xideRecoveryListClear(&list);
    cancel=1; CHECK(!xideRecoveryBackupCreate(root,&cancel,&backup,&error) && same(metadata,large,size) && same(layout,rawLayout,sizeof(rawLayout))); xideRecoveryBackupClear(&backup); cancel=0;
    wchar_t* w=xidePathNativeWide(metadata); CHECK(w); locked=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); free(w); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideRecoveryBackupCreate(root,&cancel,&backup,&error) && same(metadata,large,size) && xidePathExists(layout)); CloseHandle(locked); locked=INVALID_HANDLE_VALUE; xideRecoveryBackupClear(&backup);
    /* Destroy a completed but unconsumed repair: the UI never committed it. */
    service=xideRecoveryServiceCreate(root); CHECK(service && xideRecoveryServiceStart(service,XIDE_RECOVERY_REBUILD,NULL,&error));
    uint64_t deadline=GetTickCount64()+10000;
    while(xidePathExists(metadata) && GetTickCount64()<deadline) Sleep(2);
    CHECK(!xidePathExists(metadata)); xideRecoveryServiceDestroy(service); service=NULL;
    CHECK(same(metadata,large,size) && same(layout,rawLayout,sizeof(rawLayout)));
    service=xideRecoveryServiceCreate(root); CHECK(service && xideRecoveryServiceStart(service,XIDE_RECOVERY_REBUILD,NULL,&error));
    xideRecoveryServiceCancel(service); xideRecoveryServiceDestroy(service); service=NULL;
    CHECK(same(metadata,large,size) && same(layout,rawLayout,sizeof(rawLayout)));
    CHECK(writeText(metadata,"{\"format\":\"xlang-ide-session-1\"}") && writeText(previous,"{\"format\":\"xlang-ide-session-1\"}") && age(body) && age(base));
    CHECK(xideRecoveryLock(&guard,root,&error) && xideRecoveryCollect(&guard,now(),NULL,&stats,&error) && stats.removed==2 && !xidePathExists(body) && !xidePathExists(base) && xidePathExists(foreign));
    CHECK(xideRecoveryListRead(&guard,&list,&cancel,&error)); archived=-1; for(size_t i=0;i<list.count;++i) if(list.items[i].archived && strstr(list.items[i].reference,"1234567890123456.text")) archived=(int)i;
    CHECK(archived>=0 && xideRecoveryRead(&guard,list.items[archived].reference,&preview,&error) && !strcmp(preview.text,code));
    copyPath=xidePathResolve(archive,"manifest.json"); CHECK(copyPath && writeText(copyPath,"{damaged backup manifest") && !xideRecoveryBackupsCheck(&guard,&error));
    if(argc==2) {
        wchar_t exe[4096]; CHECK(GetModuleFileNameW(NULL,exe,4096));
        /* Keep argv ASCII; the UTF-8 deep Unicode scope is created in the child. */
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,NULL,0); CHECK(n);
        wchar_t* arg=malloc((size_t)n*sizeof(*arg)); CHECK(arg); CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,arg,n));
        size_t count=wcslen(exe)+wcslen(arg)+32; wchar_t* cmd=malloc(count*sizeof(*cmd)); CHECK(cmd);
        swprintf(cmd,count,L"\"%ls\" \"%ls\" --deep",exe,arg); free(arg);
        STARTUPINFOW start={0}; PROCESS_INFORMATION process={0}; start.cb=sizeof(start);
        int made=CreateProcessW(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&start,&process); free(cmd); CHECK(made);
        DWORD waited=WaitForSingleObject(process.hProcess,15000),code=1;
        if(waited!=WAIT_OBJECT_0) { TerminateProcess(process.hProcess,2); WaitForSingleObject(process.hProcess,5000); }
        int got=GetExitCodeProcess(process.hProcess,&code); CloseHandle(process.hThread); CloseHandle(process.hProcess);
        if(!got || waited!=WAIT_OBJECT_0 || code) fprintf(stderr,"deep recovery child wait=%lu exit=%lu\n",waited,code);
        CHECK(got && waited==WAIT_OBJECT_0 && code==0);
        puts("deep Unicode recovery child exited 0");
    }
    if(argc==3) puts("recovery_backup_test deep Unicode scope passed: >=400 UTF-16 units, complete raw backup/retirement/undo, cancellation, lock protection, scan/preview, GC and damaged archive gate");
    puts("recovery_backup_test passed: raw invalid/oversize metadata copies, complete independent bodies, cancellation/locked originals, retirement undo, worker cancellation and pending-result teardown, backed-up preview after active GC and damaged archive protection");
cleanup:
    xideRecoveryServiceDestroy(service);
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    if(backup.retired) xideRecoveryBackupUndo(&backup,&error);
    xideRecoveryBackupClear(&backup); xideRecoveryUnlock(&guard); xideRecoveryListClear(&list); xideTextFileClear(&preview);
    free(root); free(metadata); free(layout); free(previous); free(body); free(base); free(invalid); free(foreign); free(archive); free(copyPath); free(large); return failed;
}
