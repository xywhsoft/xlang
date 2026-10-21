#include "platform/startup_stage.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static int checks;
#define CHECK(value) do { ++checks; if(!(value)) {fprintf(stderr,"startup_stage_test line=%d error=%lu\n",__LINE__,GetLastError());return 1;} } while(0)
static int writeBytes(const wchar_t* path,const char* bytes)
{
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD written=0;int okay=file!=INVALID_HANDLE_VALUE && WriteFile(file,bytes,4,&written,NULL) && written==4;
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    return okay;
}
static const wchar_t* value(const wchar_t* block,const wchar_t* key)
{
    size_t n=wcslen(key);for(const wchar_t* row=block;*row;row+=wcslen(row)+1)if(!_wcsnicmp(row,key,n) && row[n]==L'=')return row+n+1;
    return NULL;
}
static void originEnvironment(const xide_startup_stage* stage)
{
    const wchar_t* keys[]={L"XLANG_IDE_STARTUP_ORIGIN",L"XLANG_IDE_STARTUP_EXE_SHA256",L"XLANG_IDE_STARTUP_DLL_SHA256"};
    for(int i=0;i<3;++i)SetEnvironmentVariableW(keys[i],stage ? value(stage->environment,keys[i]) : NULL);
}
int main(void)
{
    int count=0;wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&count);CHECK(args && count==2);
    wchar_t root[32768],program[32768],library[32768],origin[32768];
    CHECK(swprintf(root,32768,L"\\\\?\\%ls",args[1])>0);LocalFree(args);
    CHECK(CreateDirectoryW(root,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    for(int i=0;i<3;++i){size_t n=wcslen(root);CHECK(swprintf(root+n,32768-n,L"\\segment-%d-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrst",i)>0);CHECK(CreateDirectoryW(root,NULL));}
    CHECK(swprintf(program,32768,L"%ls\\original.exe",root)>0);
    CHECK(swprintf(library,32768,L"%ls\\xge.dll",root)>0);
    CHECK(xideStartupDirectoryBytes(program)>=256);
    CHECK(writeBytes(program,"EXE1") && writeBytes(library,"DLL1"));
    xide_startup_stage stage={0};DWORD error=0;
    CHECK(SetEnvironmentVariableW(L"XIDE_STAGE_UNRELATED",L"中文 😀 retained"));
    CHECK(xideStartupStagePrepare(program,&stage,&error));
    CHECK(stage.created[0] && stage.created[1] && stage.directoryCreated);
    CHECK(xideStartupDirectoryBytes(stage.program)<256);
    CHECK(!wcscmp(value(stage.environment,L"XIDE_STAGE_UNRELATED"),L"中文 😀 retained"));
    CHECK(!wcscmp(value(stage.environment,L"XLANG_IDE_STARTUP_ORIGIN"),program));
    HANDLE denied=CreateFileW(program,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(denied==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION);
    CHECK(!DeleteFileW(stage.program) && GetLastError()==ERROR_SHARING_VIOLATION);
    originEnvironment(&stage);
    CHECK(xideStartupStageOrigin(stage.program,origin,&error)==1 && !wcscmp(origin,program));
    CHECK(SetEnvironmentVariableW(L"XLANG_IDE_STARTUP_DLL_SHA256",NULL));
    CHECK(xideStartupStageOrigin(stage.program,origin,&error)==-1 && !origin[0]);
    originEnvironment(&stage);CHECK(SetEnvironmentVariableW(L"XLANG_IDE_STARTUP_EXE_SHA256",L"bad"));
    CHECK(xideStartupStageOrigin(stage.program,origin,&error)==-1 && !origin[0]);
    originEnvironment(&stage);
    CHECK(SetEnvironmentVariableW(L"XLANG_IDE_STARTUP_DLL_SHA256",L"0000000000000000000000000000000000000000000000000000000000000000"));
    CHECK(xideStartupStageOrigin(stage.program,origin,&error)==-1);
    originEnvironment(NULL);CHECK(xideStartupStageOrigin(program,origin,&error)==0);
    wchar_t copied[32768];CHECK(wcslen(stage.directory)<32768);wcscpy(copied,stage.directory);
    xideStartupStageClear(&stage);CHECK(GetFileAttributesW(copied)==INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesW(program)!=INVALID_FILE_ATTRIBUTES && GetFileAttributesW(library)!=INVALID_FILE_ATTRIBUTES);
    CHECK(xideStartupStagePrepare(program,&stage,&error));
    wchar_t replacement[32768],foreign[32768];wcscpy(copied,stage.directory);wcscpy(replacement,stage.program);
    CHECK(swprintf(foreign,32768,L"%ls\\foreign.bin",copied)>0 && writeBytes(foreign,"USER"));
    /* Keep the first file's identity alive so the filesystem cannot recycle it. */
    CloseHandle(stage.copies[0]);stage.copies[0]=NULL;
    HANDLE held=CreateFileW(replacement,FILE_READ_ATTRIBUTES,7,NULL,OPEN_EXISTING,0,NULL);CHECK(held!=INVALID_HANDLE_VALUE);
    wchar_t displaced[32768];CHECK(swprintf(displaced,32768,L"%ls\\displaced.bin",copied)>0);
    CHECK(MoveFileW(replacement,displaced) && writeBytes(replacement,"USER"));
    xideStartupStageClear(&stage);
    CHECK(GetFileAttributesW(replacement)!=INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesW(foreign)!=INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesW(copied)!=INVALID_FILE_ATTRIBUTES);
    CHECK(CloseHandle(held));
    CHECK(DeleteFileW(displaced) && DeleteFileW(replacement) && DeleteFileW(foreign) && RemoveDirectoryW(copied));
    CHECK(DeleteFileW(library));
    CHECK(!xideStartupStagePrepare(program,&stage,&error) && error!=0);
    CHECK(!stage.directory && !stage.program && !stage.environment);
    CHECK(writeBytes(library,"DLL1"));
    CHECK(xideStartupStagePrepare(program,&stage,&error));xideStartupStageClear(&stage);
    printf("startup_stage_test passed: %d native assertions; verified origin, pinned inputs, owned cleanup and fail-before-graphics\n",checks);
    return 0;
}
