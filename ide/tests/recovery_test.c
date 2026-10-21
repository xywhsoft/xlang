#include "platform/recovery_store.h"
#include "platform/state_file.h"
#include "platform/path.h"
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"recovery_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int toWide(const char* path,wchar_t* wide) { return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096); }
static uint64_t nowTicks(void) { FILETIME t; GetSystemTimeAsFileTime(&t); return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static int age(const char* path,uint64_t created,uint64_t written)
{
    wchar_t wide[4096]; if(!toWide(path,wide)) return 0;
    HANDLE h=CreateFileW(wide,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    FILETIME a={(DWORD)created,(DWORD)(created>>32)},b={(DWORD)written,(DWORD)(written>>32)};
    int okay=h!=INVALID_HANDLE_VALUE && SetFileTime(h,&a,NULL,&b); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); return okay;
}
static int writeState(const char* path,const char* body)
{ xvalue* v=xrtJsonParse(sv(body)); size_t length=0; char* s=v ? xrtJsonStringify(v,true,&length) : NULL; int okay=s && xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)s,length}); xrtFree(s); xrtValueRelease(v); return okay; }
static int probeProcess(const char* root)
{
    wchar_t exe[4096],directory[4096],command[9000]; if(!GetModuleFileNameW(NULL,exe,4096) || !toWide(root,directory)) return 0;
    swprintf(command,9000,L"\"%ls\" --lock-probe \"%ls\"",exe,directory);
    STARTUPINFOW startup={0}; startup.cb=sizeof(startup); PROCESS_INFORMATION pi={0};
    if(!CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&pi)) return 0;
    DWORD waited=WaitForSingleObject(pi.hProcess,10000),code=1;
    if(waited==WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess,&code); else { TerminateProcess(pi.hProcess,99); WaitForSingleObject(pi.hProcess,1000); }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return waited==WAIT_OBJECT_0 && code==0;
}
static int junction(const char* path,const char* target)
{
    wchar_t p[4096],t[4096],sub[4100]; if(!toWide(path,p) || !toWide(target,t)) return 0;
    for(wchar_t* c=t;*c;++c) if(*c=='/') *c='\\';
    swprintf(sub,4100,L"\\??\\%ls",t); size_t a=wcslen(sub),b=wcslen(t);
    struct { DWORD tag; WORD bytes,reserved,subOffset,subLength,printOffset,printLength; wchar_t buffer[8200]; } data={0};
    data.tag=IO_REPARSE_TAG_MOUNT_POINT; data.subLength=(WORD)(a*2); data.printOffset=(WORD)((a+1)*2); data.printLength=(WORD)(b*2); data.bytes=(WORD)(8+(a+b+2)*2);
    memcpy(data.buffer,sub,(a+1)*2); memcpy(data.buffer+a+1,t,(b+1)*2);
    if(!CreateDirectoryW(p,NULL)) return 0;
    HANDLE h=CreateFileW(p,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL); DWORD transferred=0;
    int okay=h!=INVALID_HANDLE_VALUE && DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&data,data.bytes+8,NULL,0,&transferred,NULL);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    if(!okay) RemoveDirectoryW(p);
    return okay;
}
int main(int argc,char** argv)
{
    xide_model_error error={0}; xide_recovery_guard guard={0},other={0}; xide_recovery_stats stats={0}; int failed=0;
    char *root=NULL,*current=NULL,*previous=NULL,*foreign=NULL,*alias=NULL,*scope2=NULL,*junctionPath=NULL,*archive=NULL; char* files[14]={0};
    HANDLE locked=INVALID_HANDLE_VALUE; wchar_t wide[4096],wide2[4096];
    if(argc==3 && !strcmp(argv[1],"--lock-probe")) {
        int okay=!xideRecoveryLock(&guard,argv[2],&error) && error.code==XIDE_MODEL_CONFLICT;
        xideRecoveryUnlock(&guard); return okay ? 0 : 1;
    }
    CHECK(argc==2); char* id=xideCreateId("recovery"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id);
    CHECK(root && xideRecoveryLock(&guard,root,&error));
    CHECK(!xideRecoveryLock(&other,root,&error) && error.code==XIDE_MODEL_CONFLICT && probeProcess(root));
    uint64_t now=nowTicks(),old=now-UINT64_C(9)*24*60*60*10000000;
    current=xideRecoveryPath(root,"session.json",&error); previous=xideRecoveryPath(root,"previous-session.json",&error); CHECK(current && previous);
    for(int i=0;i<11;++i) {
        char name[128]; snprintf(name,sizeof(name),"recovery/doc-00000000-1111-2222-3333-444444444444-%016llx.%s",(unsigned long long)(i+1),i==1 || i==3 ? "base" : "text");
        if(i==5) snprintf(name,sizeof(name),"recovery/snap-00000000-1111-2222-3333-444444444444.text");
        files[i]=xideRecoveryPath(root,name,&error); CHECK(files[i] && writeText(files[i],"恢复正文\n") && age(files[i],old,old));
    }
    CHECK(age(files[6],now,now) && age(files[7],now,old));
    char body[2048];
    snprintf(body,sizeof(body),"{\"format\":\"xlang-ide-session-1\",\"openDocuments\":[{\"recoveryText\":\"recovery/%s\",\"recoveryBaseline\":\"recovery/%s\"}]}",xidePathFilename(files[0]),xidePathFilename(files[1])); CHECK(writeState(current,body));
    snprintf(body,sizeof(body),"{\"format\":\"xlang-ide-session-1\",\"future\":true,\"openDocuments\":[{\"recoveryText\":\"recovery/%s\",\"recoveryBaseline\":\"recovery/%s\"}]}",xidePathFilename(files[2]),xidePathFilename(files[3])); CHECK(writeState(previous,body));
    CHECK(toWide(files[8],wide)); locked=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    alias=xidePathResolve(root,"outside-linked.txt"); CHECK(alias && toWide(alias,wide) && toWide(files[9],wide2) && CreateHardLinkW(wide,wide2,NULL));
    CHECK(toWide(files[10],wide) && SetFileAttributesW(wide,FILE_ATTRIBUTE_READONLY));
    files[11]=xideRecoveryPath(root,"recovery/user.txt",&error); files[12]=xideRecoveryPath(root,"recovery/legacy-custom.text",&error); CHECK(files[11] && files[12] && writeText(files[11],"foreign") && writeText(files[12],"legacy") && age(files[11],old,old) && age(files[12],old,old));
    files[13]=xidePathResolve(root,"recovery/doc-00000000-1111-2222-3333-444444444444-ffffffffffffffff.text"); CHECK(files[13] && toWide(files[13],wide) && CreateDirectoryW(wide,NULL));
    CHECK(xideRecoveryCollect(&guard,now,NULL,&stats,&error));
    CHECK(stats.removed==2 && stats.removedBytes==strlen("恢复正文\n")*2 && stats.referenced==4 && !xidePathExists(files[4]) && !xidePathExists(files[5]));
    for(int i=0;i<14;++i) if(i!=4 && i!=5) CHECK(xidePathExists(files[i]));
    CHECK(xidePathExists(alias));
    CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==1 && !xidePathExists(files[8]) && xidePathExists(files[9]));
    uint64_t boundary=now-UINT64_C(7)*24*60*60*10000000;
    CHECK(writeText(files[4],"grace boundary") && age(files[4],boundary+1,boundary+1));
    CHECK(xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==0 && xidePathExists(files[4]));
    CHECK(xideRecoveryCollect(&guard,now+1,NULL,&stats,&error) && stats.removed==1 && !xidePathExists(files[4]));
    CHECK(writeText(files[4],"old but newly unreferenced") && age(files[4],old,old));
    char protectedBody[1024]; snprintf(protectedBody,sizeof(protectedBody),"{\"openDocuments\":[{\"recoveryText\":\"recovery/%s\",\"recoveryBaseline\":\"recovery/%s\"}]}",xidePathFilename(files[4]),xidePathFilename(files[1]));
    xvalue* protect=xrtJsonParse(sv(protectedBody)); CHECK(protect);
    CHECK(xideRecoveryRetain(&guard,protect,now,&error)); xrtValueRelease(protect);
    CHECK(xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==0 && xidePathExists(files[4]));
    CHECK(xideRecoveryCollect(&guard,now+UINT64_C(7)*24*60*60*10000000-1,NULL,&stats,&error) && xidePathExists(files[4]));
    CHECK(xideRecoveryCollect(&guard,now+UINT64_C(7)*24*60*60*10000000,NULL,&stats,&error) && !xidePathExists(files[4]));
    CHECK(writeText(files[4],"must retain") && age(files[4],old,old));
    CHECK(writeText(previous,"{invalid") && !xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==0 && xidePathExists(files[4]));
    CHECK(writeState(previous,"{\"format\":\"xlang-ide-session-1\"}"));
    CHECK(writeState(current,"{\"format\":\"xlang-ide-session-1\",\"openDocuments\":[{\"recoveryText\":\"recovery/../outside\",\"recoveryBaseline\":\"recovery/a.base\"}]}") && !xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==0 && xidePathExists(files[4]));
    CHECK(writeState(current,"{\"format\":\"future-session-99\"}") && !xideRecoveryCollect(&guard,now,NULL,&stats,&error) && xidePathExists(files[4]));
    CHECK(writeState(current,"{\"format\":\"xlang-ide-session-1\"}"));
    archive=xidePathResolve(root,"archives/unknown-backup"); CHECK(archive && xrtDirCreateAll(archive) && !xideRecoveryCollect(&guard,now,NULL,&stats,&error) && stats.removed==0 && xidePathExists(files[4]));
    CHECK(!xideRecoveryPath(root,"../outside.txt",&error) && !xideRecoveryPath(root,"recovery/../outside.txt",&error));
    xideRecoveryUnlock(&guard);
    scope2=xidePathResolve(root,"junction-scope"); foreign=xidePathResolve(root,"foreign"); CHECK(scope2 && foreign && xrtDirCreateAll(scope2) && xrtDirCreateAll(foreign));
    junctionPath=xidePathResolve(scope2,"recovery"); CHECK(junctionPath && junction(junctionPath,foreign));
    CHECK(!xideRecoveryLock(&guard,scope2,&error) && !guard.lockHandle && !xideRecoveryPath(scope2,"recovery/session.text",&error));
    CHECK(toWide(junctionPath,wide) && RemoveDirectoryW(wide));
    puts("recovery_test passed: cross-process lock, current/previous pins, seven-day grace, creation time, locked/read-only/hardlinked/foreign files, damaged references, archive protection and junction rejection");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideRecoveryUnlock(&guard); xideRecoveryUnlock(&other);
    for(int i=0;i<14;++i) free(files[i]);
    free(root); free(current); free(previous); free(foreign); free(alias); free(scope2); free(junctionPath); free(archive); return failed;
}
