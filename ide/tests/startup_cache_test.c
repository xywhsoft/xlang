#include "platform/startup_stage.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winioctl.h>
#include <stddef.h>
#include <wchar.h>
static int checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"startup_cache_test line=%d error=%lu\n",__LINE__,GetLastError());return 1;}} while(0)
static int bytes(const wchar_t* path,const char* data,DWORD mode)
{
    HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,mode,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0;
    int okay=h!=INVALID_HANDLE_VALUE && WriteFile(h,data,4,&n,NULL) && n==4 && FlushFileBuffers(h);
    if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    return okay;
}
static int hasBytes(const wchar_t* path,const char* data)
{
    HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);char buffer[4];DWORD n=0;
    int okay=h!=INVALID_HANDLE_VALUE && ReadFile(h,buffer,sizeof(buffer),&n,NULL) && n==4 && !memcmp(buffer,data,4);
    if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    return okay;
}
static void drop(xide_startup_stage* stage)
{
    xideStartupCacheRelease(stage->cacheLease);stage->cacheLease=NULL;
    for(int i=0;i<2;++i){if(stage->originals[i])CloseHandle(stage->originals[i]);if(stage->copies[i])CloseHandle(stage->copies[i]);stage->originals[i]=stage->copies[i]=NULL;}
}
static int junction(const wchar_t* link,const wchar_t* destination)
{
    typedef struct mount_point {DWORD tag;WORD length,reserved,subOffset,subLength,printOffset,printLength;wchar_t path[1];} mount_point;
    const wchar_t* target=!wcsncmp(destination,L"\\\\?\\",4) ? destination+4 : destination;
    size_t n=wcslen(target),sub=n+4,units=sub+1+n+1;
    if(units*2+offsetof(mount_point,path)>16384 || !CreateDirectoryW(link,NULL))return 0;
    mount_point* record=calloc(1,16384);if(!record)return 0;
    record->tag=IO_REPARSE_TAG_MOUNT_POINT;record->length=(WORD)(8+units*2);record->subLength=(WORD)(sub*2);record->printOffset=(WORD)((sub+1)*2);record->printLength=(WORD)(n*2);
    memcpy(record->path,L"\\??\\",4*sizeof(wchar_t));memcpy(record->path+4,target,(n+1)*sizeof(wchar_t));memcpy(record->path+sub+1,target,(n+1)*sizeof(wchar_t));
    HANDLE file=CreateFileW(link,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);DWORD returned=0;
    int okay=file!=INVALID_HANDLE_VALUE && DeviceIoControl(file,FSCTL_SET_REPARSE_POINT,record,(DWORD)(offsetof(mount_point,path)+units*2),NULL,0,&returned,NULL);
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    free(record);return okay;
}
static int ready(const wchar_t* path,const wchar_t* text)
{
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);char* data=n>0 ? malloc((size_t)n) : NULL;
    if(!data)return 0;
    int okay=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,data,n,NULL,NULL)!=0;
    HANDLE file=okay ? CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;DWORD written=0;
    okay=file!=INVALID_HANDLE_VALUE && WriteFile(file,data,(DWORD)n-1,&written,NULL) && written==(DWORD)n-1 && FlushFileBuffers(file);
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    free(data);return okay;
}
int main(void)
{
    int count=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&count);CHECK(argv && count>=3);DWORD error=0;
    if(!wcscmp(argv[1],L"--prepare") || !wcscmp(argv[1],L"--child")) {
        CHECK(count==5);xide_startup_stage stage={0};int preparing=!wcscmp(argv[1],L"--prepare");
        if(preparing){CHECK(xideStartupStagePrepare(argv[2],&stage,&error));CHECK(ready(argv[3],stage.program));}
        else {CHECK(xideStartupCacheChildHold(argv[2],&error));CHECK(ready(argv[3],argv[2]));}
        for(int i=0;i<3000 && GetFileAttributesW(argv[4])==INVALID_FILE_ATTRIBUTES;++i)Sleep(20);
        if(preparing)xideStartupStageClear(&stage);
        LocalFree(argv);return 0;
    }
    wchar_t root[32768];CHECK(GetFullPathNameW(argv[2],32768,root,NULL)>0);
    FILETIME time;GetSystemTimeAsFileTime(&time);ULONGLONG now=((ULONGLONG)time.dwHighDateTime<<32)|time.dwLowDateTime;
    if(!wcscmp(argv[1],L"--prune")) {
        CHECK(count==4);xide_startup_cache_stats stats;ULONGLONG age=wcstoull(argv[3],NULL,10);
        CHECK(xideStartupCachePrune(root,now+age,&stats));printf("{\"scanned\":%zu,\"removed\":%zu,\"retained\":%zu,\"rejected\":%zu,\"hashedBytes\":%llu}\n",stats.scanned,stats.removed,stats.retained,stats.rejected,stats.hashedBytes);LocalFree(argv);return 0;
    }
    CHECK(!wcscmp(argv[1],L"--cases") && count==3);
    CHECK(CreateDirectoryW(root,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    CHECK(SetEnvironmentVariableW(L"TEMP",root) && SetEnvironmentVariableW(L"TMP",root));
    wchar_t source[32768],program[32768],library[32768],foreign[32768],marker[32768];CHECK(swprintf(source,32768,L"\\\\?\\%ls",root)>0);
    for(int i=0;i<3;++i){size_t n=wcslen(source);CHECK(swprintf(source+n,32768-n,L"\\source-%d-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrst",i)>0);CHECK(CreateDirectoryW(source,NULL));}
    CHECK(swprintf(program,32768,L"%ls\\original.exe",source)>0 && swprintf(library,32768,L"%ls\\xge.dll",source)>0);
    CHECK(bytes(program,"EXE1",CREATE_NEW) && bytes(library,"DLL1",CREATE_NEW));
    xide_startup_stage stage={0};xide_startup_cache_stats stats;ULONGLONG expired=now+XIDE_STARTUP_CACHE_GRACE+600000000;
    CHECK(!xideStartupCachePrune(NULL,expired,&stats) && stats.scanned==0);
    CHECK(xideStartupStagePrepare(program,&stage,&error));
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.retained==1);
    CHECK(GetFileAttributesW(stage.directory)!=INVALID_FILE_ATTRIBUTES);
    CHECK(swprintf(marker,32768,L"%ls\\.xide-startup",stage.directory)>0);
    CHECK(!DeleteFileW(marker) && GetLastError()==ERROR_SHARING_VIOLATION);
    drop(&stage);CHECK(xideStartupCachePrune(root,now,&stats) && stats.removed==0 && stats.retained==1);
    CHECK(xideStartupCachePrune(root,now-1,&stats) && stats.removed==0);
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==1 && stats.hashedBytes==8);
    CHECK(GetFileAttributesW(stage.directory)==INVALID_FILE_ATTRIBUTES && hasBytes(program,"EXE1") && hasBytes(library,"DLL1"));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));CHECK(swprintf(foreign,32768,L"%ls\\foreign.bin",stage.directory)>0 && bytes(foreign,"USER",CREATE_NEW));drop(&stage);
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(foreign,"USER") && hasBytes(stage.program,"EXE1") && hasBytes(stage.library,"DLL1"));
    CHECK(DeleteFileW(foreign));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(bytes(stage.program,"USER",OPEN_EXISTING));
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(stage.program,"USER") && hasBytes(stage.library,"DLL1") && hasBytes(program,"EXE1"));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(swprintf(marker,32768,L"%ls\\.xide-startup",stage.directory)>0);CHECK(DeleteFileW(marker));
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(stage.program,"EXE1") && hasBytes(stage.library,"DLL1"));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(swprintf(foreign,32768,L"%ls\\linked.dll",root)>0);
    CHECK(CreateHardLinkW(foreign,stage.library,NULL));CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(foreign,"DLL1") && hasBytes(stage.program,"EXE1"));CHECK(DeleteFileW(foreign));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(swprintf(marker,32768,L"%ls\\.xide-startup",stage.directory)>0);
    wchar_t moved[32768];CHECK(swprintf(moved,32768,L"%ls\\marker-retained",root)>0);
    HANDLE held=CreateFileW(marker,FILE_READ_ATTRIBUTES,7,NULL,OPEN_EXISTING,0,NULL);CHECK(held!=INVALID_HANDLE_VALUE && MoveFileW(marker,moved) && CopyFileW(moved,marker,TRUE));
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(stage.program,"EXE1") && hasBytes(stage.library,"DLL1"));CHECK(CloseHandle(held));CHECK(DeleteFileW(marker) && MoveFileW(moved,marker));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(swprintf(marker,32768,L"%ls\\.xide-startup",stage.directory)>0);
    CHECK(bytes(marker,"BAD!",OPEN_EXISTING));CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1);
    CHECK(hasBytes(stage.program,"EXE1") && hasBytes(stage.library,"DLL1"));xideStartupStageClear(&stage);
    CHECK(xideStartupStagePrepare(program,&stage,&error));drop(&stage);CHECK(swprintf(moved,32768,L"%ls\\retained-stage",root)>0);
    CHECK(MoveFileW(stage.directory,moved));CHECK(junction(stage.directory,source));
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.removed==0 && stats.rejected==1 && hasBytes(program,"EXE1") && hasBytes(library,"DLL1"));
    CHECK(RemoveDirectoryW(stage.directory) && MoveFileW(moved,stage.directory));xideStartupStageClear(&stage);
    xide_startup_stage many[65]={{0}};
    for(int i=0;i<65;++i){CHECK(xideStartupStagePrepare(program,&many[i],&error));drop(&many[i]);}
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.scanned==64 && stats.removed==64);
    CHECK(xideStartupCachePrune(root,expired,&stats) && stats.scanned==1 && stats.removed==1);
    for(int i=0;i<65;++i)xideStartupStageClear(&many[i]);
    CHECK(hasBytes(program,"EXE1") && hasBytes(library,"DLL1"));
    printf("startup_cache_test passed: %d native assertions; leases, grace, foreign/modified/missing/replaced/corrupt records, hard links, reparse target and bounded sweep\n",checks);LocalFree(argv);return 0;
}
