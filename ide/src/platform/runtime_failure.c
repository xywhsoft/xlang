#define _WIN32_WINNT 0x0601
#include "platform/runtime_failure.h"
#include "platform/failure_window.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void copyText(char* target,size_t capacity,const char* text)
{
    if(!text) text="";
    size_t n=strlen(text);if(n>=capacity)n=capacity-1;
    memcpy(target,text,n);target[n]=0;
    /* A bounded model error may already end in a partial UTF-8 character. */
    for(int trim=0;n && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,target,(int)n,NULL,0);++trim) {
        if(trim==3) { strcpy(target,"Invalid UTF-8 detail.");return; }
        target[--n]=0;
    }
    for(char* p=target;*p;++p) if((unsigned char)*p<32 || *p==127)*p=' ';
}
void xideFailureSet(xide_runtime_failure* failure,int kind,const char* domain,int code,const char* detail,const char* operation)
{
    if(!failure || failure->kind || !kind)return;
    failure->kind=kind;failure->code=code;
    copyText(failure->domain,sizeof(failure->domain),domain);
    copyText(failure->detail,sizeof(failure->detail),detail);
    copyText(failure->operation,sizeof(failure->operation),operation);
}
void xideFailureRecovery(xide_runtime_failure* failure,int recovery,const char* detail)
{
    if(!failure)return;
    failure->recovery=recovery;copyText(failure->recoveryDetail,sizeof(failure->recoveryDetail),detail);
}
static const char* stage(int kind)
{
    switch(kind) {
    case XIDE_FAILURE_ARGUMENTS:return "arguments";
    case XIDE_FAILURE_WINDOW:return "window";
    case XIDE_FAILURE_STARTUP:return "workbench";
    case XIDE_FAILURE_WORKSPACE:return "workspace";
    case XIDE_FAILURE_DOCUMENT:return "document";
    case XIDE_FAILURE_COMMAND:return "command-help";
    case XIDE_FAILURE_RENDER:return "render";
    default:return "unknown";
    }
}
static const wchar_t* reason(int kind)
{
    switch(kind) {
    case XIDE_FAILURE_ARGUMENTS:return L"启动参数无效。请检查快捷方式或启动命令。";
    case XIDE_FAILURE_WINDOW:return L"无法创建 IDE 窗口或图形环境。";
    case XIDE_FAILURE_WORKSPACE:return L"无法打开指定的工作空间或项目。请检查路径和文件内容。";
    case XIDE_FAILURE_DOCUMENT:return L"无法打开指定的文件。请检查路径、编码和访问权限。";
    case XIDE_FAILURE_COMMAND:return L"无法打开指定的命令帮助。请检查命令目录和命令标识。";
    case XIDE_FAILURE_RENDER:return L"IDE 无法继续绘制界面，已停止运行。";
    default:return L"IDE 工作界面初始化失败。";
    }
}
static const wchar_t* recoveryText(int recovery)
{
    switch(recovery) {
    case XIDE_FAILURE_RECOVERY_DISABLED:return L"本次运行已关闭会话恢复，未写入恢复检查点。";
    case XIDE_FAILURE_RECOVERY_SAVED:return L"已保存可恢复的文档快照。重启后可在恢复管理器中查看。";
    case XIDE_FAILURE_RECOVERY_FAILED:return L"未能保存新的恢复检查点。请保留原恢复区，已有记录仍可检查。";
    default:return L"本次失败未生成新的恢复检查点。已有恢复记录可在重启后检查。";
    }
}
static char* fromWide(const wchar_t* text)
{
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);
    char* result=n ? malloc((size_t)n) : NULL;
    if(result && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,result,n,NULL,NULL)) { free(result);result=NULL; }
    return result;
}
static char* defaultRoot(int fallback)
{
    wchar_t buffer[32768];DWORD n=fallback ? GetEnvironmentVariableW(L"LOCALAPPDATA",buffer,32768) : GetModuleFileNameW(NULL,buffer,32768);
    if(!n || n>=32768)return NULL;
    char* text=fromWide(fallback ? buffer : xideStartupResourceModule(buffer)),*base=fallback ? text : text ? xidePathDirectory(text) : NULL;
    char* root=base ? xidePathResolveLexical(base,fallback ? "xlang-ide/diagnostics" : "option/diagnostics") : NULL;
    if(base!=text)free(base);
    free(text);return root;
}
static DWORD ensureDirectory(const char* path,int depth)
{
    if(depth>128)return ERROR_DIRECTORY;
    wchar_t* native=xidePathNativeWide(path);if(!native)return ERROR_NOT_ENOUGH_MEMORY;
    DWORD attributes=GetFileAttributesW(native),error=ERROR_SUCCESS;
    if(attributes!=INVALID_FILE_ATTRIBUTES) {
        if(!(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))error=ERROR_DIRECTORY;
    } else {
        error=GetLastError();
        if(error==ERROR_PATH_NOT_FOUND || error==ERROR_FILE_NOT_FOUND) {
            char* parent=xidePathDirectory(path);
            error=parent && !xidePathLexicalEqual(parent,path) ? ensureDirectory(parent,depth+1) : ERROR_DIRECTORY;
            free(parent);
            if(!error && !CreateDirectoryW(native,NULL)) {
                error=GetLastError();
                if(error==ERROR_ALREADY_EXISTS) {
                    attributes=GetFileAttributesW(native);
                    error=attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY) && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT) ? ERROR_SUCCESS : ERROR_DIRECTORY;
                }
            }
        }
    }
    free(native);return error;
}
static DWORD quota(const char* root,size_t bodySize)
{
    char* pattern=xidePathResolveLexical(root,"*");wchar_t* native=pattern ? xidePathNativeWide(pattern) : NULL;free(pattern);
    if(!native)return ERROR_NOT_ENOUGH_MEMORY;
    WIN32_FIND_DATAW entry;HANDLE search=FindFirstFileW(native,&entry);free(native);
    if(search==INVALID_HANDLE_VALUE)return GetLastError();
    unsigned files=0;unsigned long long bytes=0;DWORD result=ERROR_SUCCESS;
    do {
        if(!wcscmp(entry.cFileName,L".") || !wcscmp(entry.cFileName,L".."))continue;
        if(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) { result=ERROR_DIRECTORY;break; }
        bytes+=((unsigned long long)entry.nFileSizeHigh<<32)|entry.nFileSizeLow;
        if(wcscmp(entry.cFileName,L".diagnostic.lock"))++files;
        if(files>=XIDE_DIAGNOSTIC_MAX_FILES || bytes+bodySize>XIDE_DIAGNOSTIC_MAX_BYTES) { result=ERROR_DISK_FULL;break; }
    } while(FindNextFileW(search,&entry));
    if(!result && GetLastError()!=ERROR_NO_MORE_FILES)result=GetLastError();
    FindClose(search);return result;
}
static DWORD writeAt(const char* root,const char* body,size_t size,xide_failure_report* report)
{
    DWORD error=ensureDirectory(root,0);if(error)return error;
    wchar_t* directory=xidePathNativeWide(root);
    HANDLE pin=directory ? CreateFileW(directory,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
    DWORD openError=directory ? GetLastError() : ERROR_NOT_ENOUGH_MEMORY;
    free(directory);if(pin==INVALID_HANDLE_VALUE)return openError;
    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(pin,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) { CloseHandle(pin);return ERROR_DIRECTORY; }
    char* lockPath=xidePathResolveLexical(root,".diagnostic.lock");wchar_t* lockNative=lockPath ? xidePathNativeWide(lockPath) : NULL;free(lockPath);
    HANDLE lock=lockNative ? CreateFileW(lockNative,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE;
    openError=lockNative ? GetLastError() : ERROR_NOT_ENOUGH_MEMORY;free(lockNative);
    if(lock==INVALID_HANDLE_VALUE) { CloseHandle(pin);return openError; }
    if(!GetFileInformationByHandle(lock,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))error=ERROR_DIRECTORY;
    else error=quota(root,size);
    if(!error) {
        static LONG serial;SYSTEMTIME utc;GetSystemTime(&utc);
        char name[160];snprintf(name,sizeof(name),"xlang-ide-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%ld.log",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,GetCurrentProcessId(),InterlockedIncrement(&serial));
        char* path=xidePathResolveLexical(root,name);wchar_t* native=path ? xidePathNativeWide(path) : NULL;
        HANDLE file=native ? CreateFileW(native,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
        openError=native ? GetLastError() : ERROR_NOT_ENOUGH_MEMORY;free(native);
        if(file==INVALID_HANDLE_VALUE)error=openError;
        else {
            DWORD written=0;
            if(!WriteFile(file,body,(DWORD)size,&written,NULL) || written!=size || !FlushFileBuffers(file))error=GetLastError() ? GetLastError() : ERROR_WRITE_FAULT;
            CloseHandle(file);
            if(!error && strlen(path)<sizeof(report->path))strcpy(report->path,path);
            else if(!error)error=ERROR_FILENAME_EXCED_RANGE;
        }
        free(path);
    }
    CloseHandle(lock);CloseHandle(pin);return error;
}
int xideFailureWrite(const xide_runtime_failure* failure,const char* root,int frames,xide_failure_report* report)
{
    if(!report)return 0;
    memset(report,0,sizeof(*report));
    if(!failure || !failure->kind) { report->error=ERROR_INVALID_PARAMETER;return 0; }
    SYSTEMTIME utc;GetSystemTime(&utc);char body[XIDE_DIAGNOSTIC_MAX_REPORT];
    int n=snprintf(body,sizeof(body),"xlang-ide-runtime-failure-1\nutc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\npid=%lu\nstage=%s\ndomain=%s\ncode=%d\nframes=%d\ndetail=%s\noperation=%s\nrecovery=%d\nrecovery-detail=%s\n",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,GetCurrentProcessId(),stage(failure->kind),failure->domain,failure->code,frames,failure->detail,failure->operation,failure->recovery,failure->recoveryDetail);
    if(n<0 || (size_t)n>=sizeof(body)) { report->error=ERROR_BUFFER_OVERFLOW;return 0; }
    char* resolved=root ? xidePathResolveLexical(NULL,root) : defaultRoot(0);
    report->error=resolved ? writeAt(resolved,body,(size_t)n,report) : ERROR_NOT_ENOUGH_MEMORY;free(resolved);
    /* Quota exhaustion, a busy writer and memory failure never spill elsewhere. */
    if(!root && report->error && report->error!=ERROR_DISK_FULL && report->error!=ERROR_SHARING_VIOLATION && report->error!=ERROR_NOT_ENOUGH_MEMORY) {
        resolved=defaultRoot(1);if(resolved) { report->fallback=1;report->error=writeAt(resolved,body,(size_t)n,report);free(resolved); }
    }
    return report->error==ERROR_SUCCESS;
}
int xideFailureMessage(const xide_runtime_failure* failure,const xide_failure_report* report,wchar_t* message,size_t capacity)
{
    if(!failure || !report || !message || capacity<4096)return 0;
    wchar_t detail[1024],domain[32],recovery[512],path[32768];
    MultiByteToWideChar(CP_UTF8,0,failure->detail,-1,detail,1024);
    MultiByteToWideChar(CP_UTF8,0,failure->domain,-1,domain,32);
    MultiByteToWideChar(CP_UTF8,0,failure->recoveryDetail,-1,recovery,512);
    MultiByteToWideChar(CP_UTF8,0,report->path,-1,path,32768);
    int n=report->error ? swprintf(message,capacity,L"%ls\n\n%ls\n错误代码：%ls %d\n\n%ls\n%ls\n\n诊断文件未能写入（Windows 错误 %lu）。原错误原因和退出状态已保留。",reason(failure->kind),detail,domain,failure->code,recoveryText(failure->recovery),recovery,report->error) : swprintf(message,capacity,L"%ls\n\n%ls\n错误代码：%ls %d\n\n%ls\n%ls\n\n诊断文件：\n%ls",reason(failure->kind),detail,domain,failure->code,recoveryText(failure->recovery),recovery,path);
    return n>=0 && (size_t)n<capacity;
}
void xideFailureShow(const xide_runtime_failure* failure,const xide_failure_report* report)
{
    wchar_t message[40000];
    if(!xideFailureMessage(failure,report,message,40000))wcscpy(message,L"xlang IDE 运行失败。诊断提示无法完整生成，请检查恢复记录。");
    xideFailureWindowShow(message);
}
