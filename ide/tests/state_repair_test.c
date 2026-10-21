#define _WIN32_WINNT 0x0601
#include "platform/state_repair.h"
#include "platform/file_rename.h"
#include "platform/path.h"
#include "services/recovery.h"
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int rejectRename;
static BOOL checkedRename(HANDLE h,FILE_INFO_BY_HANDLE_CLASS kind,LPVOID info,DWORD size)
{
    if(kind==FileRenameInfo && rejectRename>0 && --rejectRename==0) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return SetFileInformationByHandle(h,kind,info,size);
}
/* Exercise the production transaction's OS-failure rollback, retaining actual
   file handles and real Win32 renames for preparation, retirement and undo. */
#define SetFileInformationByHandle checkedRename
#include "platform/state_repair.c"
#undef SetFileInformationByHandle
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"state_repair_test:%d: %s; %s; Win32=%lu\n",__LINE__,#v,error.message,GetLastError()); failed=1; goto cleanup; } } while(0)
static int tWrite(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int tSame(const char* p,const unsigned char* bytes,size_t length)
{ size_t n=0; unsigned char* raw=xrtFileReadAllLimit(p,16u*1024u*1024u,&n); int okay=raw && n==length && !memcmp(raw,bytes,n); xrtFree(raw); return okay; }
static int tJunction(const char* path,const char* target)
{
    wchar_t p[4096],t[4096],sub[4100]; if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,p,4096) || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,target,-1,t,4096)) return 0;
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
    int failed=0,checks=0; xide_model_error error={0}; xide_state_repair repair={0}; xide_state_file state={0}; xide_file_snapshot snap={0};
    char *root=NULL,*path=NULL,*other=NULL,*lockPath=NULL,*rawPath=NULL,*manifestPath=NULL,*staging=NULL; unsigned char* large=NULL; HANDLE locked=INVALID_HANDLE_VALUE;
    xide_recovery_service* service=NULL; xide_recovery_result result={0}; xvalue* desired=xrtValueObject();
    CHECK(argc==2); char* id=xideCreateId("state-repair"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id);
    CHECK(root && xrtDirCreateAll(root)); path=xidePathResolve(root,"中文 设置.json"); other=xidePathResolve(root,"other.json"); lockPath=path ? malloc(strlen(path)+6) : NULL; if(lockPath) sprintf(lockPath,"%s.lock",path);
    CHECK(path && other && lockPath && desired && put(desired,"format",xrtValueString(sv("xlang-ide-settings-1"))) && put(desired,"future",xrtValueInt(77)));
    size_t count=8u*1024u*1024u+17; large=malloc(count); CHECK(large); memset(large,0xff,count); memcpy(large,"raw invalid metadata",20);
    CHECK(xrtFileWriteAtomic(path,(xbytesview){large,count}) && xideFileSnapshotInspect(path,&snap,&error));
    CHECK(xideStateRepairPrepare(path,NULL,&repair,&error) && repair.complete && repair.existed && !repair.retired && repair.bytes==count);
    rawPath=xidePathResolve(repair.directory,"original.bin"); manifestPath=xidePathResolve(repair.directory,"manifest.json");
    CHECK(rawPath && manifestPath && tSame(rawPath,large,count) && xideStateOpen(&state,manifestPath,"xlang-ide-state-backup-1",&error));
    char digest[65]; for(int i=0;i<32;++i) snprintf(digest+i*2,3,"%02x",snap.sha256[i]); xstrview stored={0};
    CHECK(xrtValueGetString(xrtValueObjectGet(state.source,sv("sha256")),&stored) && stored.Size==64 && !memcmp(stored.Data,digest,64)); xideStateClear(&state);
    xide_state_file missing={0}; CHECK(xideStateOpen(&missing,other,"xlang-ide-settings-1",&error)); free(missing.path); missing.path=xidePathResolve(NULL,path);
    CHECK(!xideStateWrite(&missing,desired,&error) && error.code==XIDE_MODEL_CONFLICT); xideStateClear(&missing);
    CHECK(xideStateRepairPublish(&repair,desired,&error) && repair.published && repair.retired);
    xideStateRepairClear(&repair); CHECK(xideStateOpen(&state,path,"xlang-ide-settings-1",&error)); xideStateClear(&state);
    CHECK(tSame(rawPath,large,count)); free(rawPath); rawPath=NULL; free(manifestPath); manifestPath=NULL;
    const char* damaged="{corrupt original"; CHECK(tWrite(path,damaged));
    volatile LONG cancel=1; CHECK(!xideStateRepairPrepare(path,&cancel,&repair,&error) && tSame(path,(unsigned char*)damaged,strlen(damaged))); xideStateRepairClear(&repair);
    wchar_t* w=wide(path); locked=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); free(w); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideStateRepairPrepare(path,NULL,&repair,&error) && tSame(path,(unsigned char*)damaged,strlen(damaged))); CloseHandle(locked); locked=INVALID_HANDLE_VALUE; xideStateRepairClear(&repair);
    w=wide(lockPath); locked=CreateFileW(w,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,0,NULL); free(w); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideStateRepairPrepare(path,NULL,&repair,&error) && error.code==XIDE_MODEL_CONFLICT); CloseHandle(locked); locked=INVALID_HANDLE_VALUE; xideStateRepairClear(&repair);
    w=wide(path); CHECK(SetFileAttributesW(w,FILE_ATTRIBUTE_READONLY)); free(w);
    CHECK(!xideStateRepairPrepare(path,NULL,&repair,&error)); xideStateRepairClear(&repair); w=wide(path); CHECK(SetFileAttributesW(w,FILE_ATTRIBUTE_NORMAL)); free(w);
    w=wide(other); wchar_t* original=wide(path); CHECK(CreateHardLinkW(w,original,NULL)); free(original);
    CHECK(!xideStateRepairPrepare(path,NULL,&repair,&error)); xideStateRepairClear(&repair); CHECK(DeleteFileW(w)); free(w);
    /* All native resources unwind when a completed worker result is discarded. */
    service=xideRecoveryServiceCreate(NULL); CHECK(service && xideRecoveryServiceStart(service,XIDE_RECOVERY_GLOBAL,path,&error));
    uint64_t deadline=GetTickCount64()+10000; while(GetTickCount64()<deadline && !xideRecoveryServiceTake(service,&result)) Sleep(2);
    CHECK(result.okay && result.global.complete && !result.global.retired); xideRecoveryResultClear(&result,1); xideRecoveryServiceDestroy(service); service=NULL;
    CHECK(tSame(path,(unsigned char*)damaged,strlen(damaged)) && xideStateRepairPrepare(path,NULL,&repair,&error));
    rejectRename=2; CHECK(!xideStateRepairPublish(&repair,desired,&error) && !repair.retired && !repair.published); xideStateRepairClear(&repair);
    CHECK(tSame(path,(unsigned char*)damaged,strlen(damaged)));
    CHECK(xideStateRepairPrepare(path,NULL,&repair,&error)); staging=xidePathResolve(repair.directory,"replacement.json"); CHECK(staging && tWrite(staging,"foreign staging"));
    CHECK(!xideStateRepairPublish(&repair,desired,&error) && !repair.retired); xideStateRepairClear(&repair); free(staging); staging=NULL;
    CHECK(tSame(path,(unsigned char*)damaged,strlen(damaged)));
    /* An originally missing file created by another writer must not be replaced. */
    CHECK(xideStateRepairPrepare(other,NULL,&repair,&error) && !repair.existed && repair.complete);
    CHECK(tWrite(other,"other writer") && !xideStateRepairPublish(&repair,desired,&error) && !repair.published); xideStateRepairClear(&repair);
    CHECK(tSame(other,(unsigned char*)"other writer",12));
    /* Linked lock files are rejected independently of the protected source. */
    char* link=xidePathResolve(root,"linked-lock"); w=wide(lockPath); original=wide(link); CHECK(link && CreateHardLinkW(original,w,NULL)); free(w); free(original);
    CHECK(!xideStateRepairPrepare(path,NULL,&repair,&error)); xideStateRepairClear(&repair); original=wide(link); CHECK(DeleteFileW(original)); free(original); free(link);
    char* symbolic=xidePathResolve(root,"parent-link"); CHECK(symbolic && tJunction(symbolic,root));
    char* linkedChild=xidePathResolveLexical(symbolic,"child.json");
    CHECK(linkedChild && !xideStateRepairPrepare(linkedChild,NULL,&repair,&error)); xideStateRepairClear(&repair);
    char* external=xidePathResolve(root,"child.json"); CHECK(external && !xidePathExists(external)); free(external);
    w=wide(symbolic); CHECK(RemoveDirectoryW(w)); free(w); free(symbolic); free(linkedChild);
    symbolic=xidePathResolve(root,"target-link.json"); CHECK(symbolic && tJunction(symbolic,root));
    CHECK(!xideStateRepairPrepare(symbolic,NULL,&repair,&error)); xideStateRepairClear(&repair); w=wide(symbolic); CHECK(RemoveDirectoryW(w)); free(w); free(symbolic);
    service=xideRecoveryServiceCreate(NULL); CHECK(service && xideRecoveryServiceStart(service,XIDE_RECOVERY_GLOBAL,path,&error));
    xideRecoveryServiceCancel(service); xideRecoveryServiceDestroy(service); service=NULL;
    CHECK(tSame(path,(unsigned char*)damaged,strlen(damaged)) && xideStateRepairPrepare(path,NULL,&repair,&error)); xideStateRepairClear(&repair);
    printf("state_repair_test passed: %d checks; raw oversize/invalid UTF-8 backup SHA-256, exclusive common writer lock, read-only/hard-link/locked preservation, cancellation/pending disposal, real retirement rollback after injected OS publish failure, missing-file race and staging collision\n",checks);
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideStateClear(&state); xideStateRepairClear(&repair); xideRecoveryResultClear(&result,1); xideRecoveryServiceDestroy(service);
    xrtValueRelease(desired); free(root); free(path); free(other); free(lockPath); free(rawPath); free(manifestPath); free(staging); free(large); return failed;
}
