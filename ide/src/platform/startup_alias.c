#include "platform/startup_alias.h"
#include "platform/startup_stage.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static wchar_t resourceModule[32768];
const wchar_t* xideStartupResourceModule(const wchar_t* actual) {return *resourceModule ? resourceModule : actual;}
static int sameFile(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{
    return !(a->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(b->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) &&
        a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow &&
        a->nFileSizeHigh==b->nFileSizeHigh && a->nFileSizeLow==b->nFileSizeLow;
}
static wchar_t* quotedCommand(const wchar_t* alias)
{
    int count=0;wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!arguments || count<1) {if(arguments)LocalFree(arguments);return NULL;}
    size_t capacity=1;
    for(int i=0;i<count;++i) {
        const wchar_t* source=i ? arguments[i] : alias;
        int quoted=!i || !*source || wcspbrk(source,L" \t\"")!=NULL;
        size_t needed=(i ? 1u : 0u)+(quoted ? 2u : 0u);
        if(!quoted)needed+=wcslen(source);
        else while(*source) {
            size_t slashes=0;while(source[slashes]==L'\\')++slashes;source+=slashes;
            needed+=(*source==L'"' || !*source) ? slashes*2 : slashes;
            if(!*source)break;
            needed+=*source==L'"' ? 2u : 1u;++source;
        }
        if(needed>32767-capacity) {LocalFree(arguments);SetLastError(ERROR_FILENAME_EXCED_RANGE);return NULL;}
        capacity+=needed;
    }
    wchar_t* command=malloc(capacity*sizeof(*command));
    if(!command) {LocalFree(arguments);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
    size_t at=0;
    for(int i=0;i<count;++i) {
        const wchar_t* source=i ? arguments[i] : alias;
        if(i)command[at++]=L' ';
        if(i && *source && !wcspbrk(source,L" \t\"")) {
            size_t length=wcslen(source);memcpy(command+at,source,length*sizeof(*source));at+=length;continue;
        }
        command[at++]=L'"';
        while(*source) {
            size_t slashes=0;while(source[slashes]==L'\\')++slashes;source+=slashes;
            size_t copies=(*source==L'"' || !*source) ? slashes*2 : slashes;
            while(copies--)command[at++]=L'\\';
            if(!*source)break;
            if(*source==L'"')command[at++]=L'\\';
            command[at++]=*source++;
        }
        command[at++]=L'"';
    }
    command[at]=0;LocalFree(arguments);return command;
}
static int startChild(const wchar_t* program,wchar_t* environment,int* exitCode,unsigned long* error)
{
    wchar_t* command=quotedCommand(program);DWORD commandError=GetLastError();STARTUPINFOW startup={0},prior={0};PROCESS_INFORMATION child={0};
    startup.cb=sizeof(startup);GetStartupInfoW(&prior);startup.dwFlags=prior.dwFlags&STARTF_USESHOWWINDOW;startup.wShowWindow=prior.wShowWindow;
    wchar_t desktop[256];DWORD desktopBytes=0;
    if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&desktopBytes)) {
        DWORD failure=GetLastError();free(command);if(error)*error=failure;return -1;
    }
    startup.lpDesktop=desktop;
    HANDLE handles[3]={GetStdHandle(STD_INPUT_HANDLE),GetStdHandle(STD_OUTPUT_HANDLE),GetStdHandle(STD_ERROR_HANDLE)};DWORD flags[3]={0};int inherit=1;
    for(int i=0;i<3;++i)if(!handles[i] || handles[i]==INVALID_HANDLE_VALUE || !GetHandleInformation(handles[i],flags+i))inherit=0;
    if(inherit) {
        startup.dwFlags|=STARTF_USESTDHANDLES;startup.hStdInput=handles[0];startup.hStdOutput=handles[1];startup.hStdError=handles[2];
        for(int i=0;i<3;++i)if(!SetHandleInformation(handles[i],HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT))inherit=0;
    }
    DWORD failure=0;int started=0;
    if(command && (inherit || !(startup.dwFlags&STARTF_USESTDHANDLES)))started=CreateProcessW(program,command,NULL,NULL,inherit,environment ? CREATE_UNICODE_ENVIRONMENT : 0,environment,NULL,&startup,&child);
    failure=started ? 0 : (command ? GetLastError() : commandError);if(!failure)failure=ERROR_NOT_ENOUGH_MEMORY;
    if(startup.dwFlags&STARTF_USESTDHANDLES)for(int i=0;i<3;++i)SetHandleInformation(handles[i],HANDLE_FLAG_INHERIT,flags[i]&HANDLE_FLAG_INHERIT);
    free(command);
    if(!started) {if(error)*error=failure;return -1;}
    CloseHandle(child.hThread);DWORD code=1;
    DWORD waited=WaitForSingleObject(child.hProcess,INFINITE);
    int completed=waited==WAIT_OBJECT_0 && GetExitCodeProcess(child.hProcess,&code);
    failure=completed ? 0 : GetLastError();CloseHandle(child.hProcess);
    if(!completed) {if(error)*error=failure;return -1;}
    if(exitCode)*exitCode=(int)code;
    return 1;
}
int xideStartupAliasMaybe(int* exitCode,unsigned long* error)
{
    if(exitCode)*exitCode=0;
    if(error)*error=0;
    resourceModule[0]=0;
    wchar_t original[32768],alias[32768];DWORD length=GetModuleFileNameW(NULL,original,32768),failure=0;
    if(!length || length>=32768)return 0;
    int context=xideStartupStageOrigin(original,resourceModule,&failure);
    if(context>0 && xideStartupCacheChildHold(original,&failure) && xideStartupStageForgetOrigin(&failure))return 0;
    if(context){if(error)*error=failure;return -1;}
    xideStartupCacheMaintain();
    if(xideStartupDirectoryBytes(original)<256)return 0;
    length=GetShortPathNameW(original,alias,32768);
    if(length && length<32768 && xideStartupDirectoryBytes(alias)>0 && xideStartupDirectoryBytes(alias)<256 && wcscmp(original,alias)) {
        HANDLE source=CreateFileW(original,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL),target=CreateFileW(alias,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        BY_HANDLE_FILE_INFORMATION a={0},b={0};int valid=source!=INVALID_HANDLE_VALUE && target!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(source,&a) && GetFileInformationByHandle(target,&b) && sameFile(&a,&b);
        int result=-1;if(valid)result=startChild(alias,NULL,exitCode,error);else if(error)*error=GetLastError() ? GetLastError() : ERROR_FILE_INVALID;
        if(source!=INVALID_HANDLE_VALUE)CloseHandle(source);
        if(target!=INVALID_HANDLE_VALUE)CloseHandle(target);
        return result;
    }
    xide_startup_stage stage={0};if(!xideStartupStagePrepare(original,&stage,&failure)){if(error)*error=failure;return -1;}
    int result=startChild(stage.program,stage.environment,exitCode,error);xideStartupStageClear(&stage);return result;
}
