#define _WIN32_WINNT 0x0601
#include "platform/runtime_failure.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static int checks;
#define CHECK(expr) do { ++checks;if(!(expr)) { fprintf(stderr,"runtime_failure_test:%d: %s (Win32=%lu)\n",__LINE__,#expr,GetLastError());return 1; } } while(0)
static char* utf8(const wchar_t* text)
{
    int n=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL);char* result=n ? malloc((size_t)n) : NULL;
    if(result)WideCharToMultiByte(CP_UTF8,0,text,-1,result,n,NULL,NULL);
    return result;
}
static int create(const char* path,const void* bytes,size_t size)
{
    wchar_t* native=xidePathNativeWide(path);HANDLE file=native ? CreateFileW(native,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;free(native);
    if(file==INVALID_HANDLE_VALUE)return 0;
    DWORD n=0;int okay=WriteFile(file,bytes,(DWORD)size,&n,NULL) && n==size;CloseHandle(file);return okay;
}
static size_t read(const char* path,char* bytes,size_t capacity)
{
    wchar_t* native=xidePathNativeWide(path);HANDLE file=native ? CreateFileW(native,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;free(native);
    if(file==INVALID_HANDLE_VALUE)return 0;
    DWORD n=0;int okay=ReadFile(file,bytes,(DWORD)capacity-1,&n,NULL);CloseHandle(file);bytes[n]=0;return okay ? n : 0;
}
static int storage(const wchar_t* directory)
{
    char* root=utf8(directory);CHECK(root);
    xide_runtime_failure failure={0};xide_failure_report report={0},second={0};wchar_t message[40000];char bytes[8192],before[8192];
    xideFailureSet(&failure,XIDE_FAILURE_WORKSPACE,"model",2,"缺失 工作空间 😀.xlw\nInjected line", "xideWorkspaceOpen");
    xideFailureSet(&failure,XIDE_FAILURE_RENDER,"XUI",-9,"Must not replace first cause","ignored");
    char* path=xidePathResolve(root,"中文 诊断 😀");CHECK(path);
    CHECK(xideFailureWrite(&failure,path,0,&report));size_t n=read(report.path,before,sizeof(before));
    CHECK(n && n<XIDE_DIAGNOSTIC_MAX_REPORT && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,before,(int)n,NULL,0));
    CHECK(strstr(before,"stage=workspace\n") && strstr(before,"code=2\n") && strstr(before,"缺失 工作空间 😀.xlw Injected line\n") && !strstr(before,"Must not replace"));
    CHECK(xideFailureMessage(&failure,&report,message,40000) && wcsstr(message,L"无法打开指定的工作空间") && wcsstr(message,L"本次失败未生成新的恢复检查点") && !wcsstr(message,L"xideWorkspaceOpen"));
    CHECK(xideFailureWrite(&failure,path,0,&second) && strcmp(report.path,second.path));
    CHECK(read(report.path,bytes,sizeof(bytes))==n && !memcmp(bytes,before,n));free(path);
    path=xidePathResolve(root,"quota");CHECK(path && xideFailureWrite(&failure,path,0,&report));n=read(report.path,before,sizeof(before));CHECK(n);
    for(unsigned i=0;i<XIDE_DIAGNOSTIC_MAX_FILES-1;++i) { char name[64];snprintf(name,sizeof(name),"existing-user-file-%u.txt",i);char* file=xidePathResolve(path,name);CHECK(file && create(file,"preserve",8));free(file); }
    CHECK(!xideFailureWrite(&failure,path,0,&second) && second.error==ERROR_DISK_FULL && !second.fallback && !*second.path);
    CHECK(xideFailureMessage(&failure,&second,message,40000) && wcsstr(message,L"诊断文件未能写入") && wcsstr(message,L"无法打开指定的工作空间"));
    CHECK(read(report.path,bytes,sizeof(bytes))==n && !memcmp(bytes,before,n));free(path);
    path=xidePathResolve(root,"byte-quota");CHECK(path && xideFailureWrite(&failure,path,0,&report));
    char* large=calloc(XIDE_DIAGNOSTIC_MAX_BYTES,1);char* userFile=xidePathResolve(path,"existing-large-file.bin");CHECK(large && userFile);memcpy(large,"preserve",8);
    CHECK(create(userFile,large,XIDE_DIAGNOSTIC_MAX_BYTES));free(large);
    CHECK(!xideFailureWrite(&failure,path,0,&second) && second.error==ERROR_DISK_FULL);
    CHECK(read(userFile,bytes,sizeof(bytes))==sizeof(bytes)-1 && !memcmp(bytes,"preserve",8));free(userFile);free(path);
    path=xidePathResolve(root,"locked");CHECK(path && xideFailureWrite(&failure,path,0,&report));
    char* lockPath=xidePathResolve(path,".diagnostic.lock");wchar_t* native=lockPath ? xidePathNativeWide(lockPath) : NULL;
    HANDLE lock=native ? CreateFileW(native,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
    free(native);free(lockPath);CHECK(lock!=INVALID_HANDLE_VALUE);
    CHECK(!xideFailureWrite(&failure,path,0,&second) && second.error==ERROR_SHARING_VIOLATION && !second.fallback);CloseHandle(lock);free(path);
    path=xidePathResolve(root,"explicit-root-is-existing-file.txt");CHECK(path && create(path,"preserve original bytes",23));
    CHECK(!xideFailureWrite(&failure,path,0,&second) && second.error==ERROR_DIRECTORY && !second.fallback);
    CHECK(read(path,bytes,sizeof(bytes))==23 && !memcmp(bytes,"preserve original bytes",23));free(path);
    path=xidePathResolve(root,"deep");CHECK(path);
    for(int i=0;i<14;++i) { char* deeper=xidePathResolve(path,"long-diagnostic-directory-中文");free(path);path=deeper;CHECK(path); }
    CHECK(strlen(path)>260 && xideFailureWrite(&failure,path,3,&report));
    CHECK(read(report.path,bytes,sizeof(bytes))>0 && strstr(bytes,"frames=3\n"));free(path);
    xide_runtime_failure bounded={0};char longDetail[1600];memset(longDetail,'x',sizeof(longDetail));memcpy(longDetail+1021,"😀",4);longDetail[1599]=0;
    xideFailureSet(&bounded,XIDE_FAILURE_RENDER,"XUI",-5,longDetail,"allocation");
    CHECK(strlen(bounded.detail)==1021 && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bounded.detail,-1,NULL,0));
    free(root);printf("runtime_failure_test storage passed: %d checks; bounded-files=64 bounded-bytes=524288 UTF-8=valid\n",checks);return 0;
}
typedef struct launch_scan { DWORD pid;const wchar_t* directory;int dialogs,failed; } launch_scan;
static int visibleText(HWND window,const RECT* outer,const unsigned char* pixels,int width,int height)
{
    HWND label=GetDlgItem(window,65535);RECT rect;
    if(GetPropW(window,L"XlangFailureText")) { if(!GetWindowRect(window,&rect))return 0;rect.left+=20;rect.right-=20;rect.top+=70;rect.bottom-=90; }
    else if(!label || !GetWindowRect(label,&rect))return 0;
    int left=rect.left-outer->left,top=rect.top-outer->top,right=rect.right-outer->left,bottom=rect.bottom-outer->top;
    if(left<0 || top<0 || right>width || bottom>height || right<=left || bottom<=top)return 0;
    unsigned light=0,dark=0;
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x) {
        const unsigned char* p=pixels+((size_t)y*(size_t)width+(size_t)x)*4;
        if(p[0]>190 && p[1]>190 && p[2]>190)++light;
        if(p[0]<120 && p[1]<120 && p[2]<120)++dark;
    }
    return light>=32 && dark>=16;
}
static int renderedCapture(HWND window,const wchar_t* directory)
{
    int width=(int)(INT_PTR)GetPropW(window,L"XlangFailureFrameWidth"),height=(int)(INT_PTR)GetPropW(window,L"XlangFailureFrameHeight");
    if(width<200 || height<150 || width>4000 || height>4000)return 0;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);HANDLE process=OpenProcess(PROCESS_VM_READ,FALSE,pid);if(!process)return 0;
    size_t size=(size_t)width*(size_t)height*4;unsigned char *pixels=malloc(size),*second=malloc(size);int okay=0;
    for(int attempt=0;pixels && second && attempt<25 && !okay;++attempt) {
        const void* frame=GetPropW(window,L"XlangFailureFrame");SIZE_T a=0,b=0;
        if(frame && ReadProcessMemory(process,frame,pixels,size,&a) && ReadProcessMemory(process,frame,second,size,&b) && a==size && b==size && !memcmp(pixels,second,size) && frame==GetPropW(window,L"XlangFailureFrame"))okay=1;
        if(!okay)Sleep(10);
    }
    unsigned light=0,dark=0;
    if(okay) for(int y=45;y<height-60;++y)for(int x=20;x<width-20;++x) { const unsigned char* p=pixels+((size_t)y*(size_t)width+(size_t)x)*4;if(p[0]>190 && p[1]>190 && p[2]>190)++light;if(p[0]<120 && p[1]<120 && p[2]<120)++dark; }
    okay=okay && light>=32 && dark>=16;
    if(pixels) {
        for(size_t i=0;i<size;i+=4) { unsigned char red=pixels[i];pixels[i]=pixels[i+2];pixels[i+2]=red; }
        BITMAPINFOHEADER info={0};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
        BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=(DWORD)(header.bfOffBits+size);
        unsigned char* bytes=malloc(header.bfSize);wchar_t path[32768];swprintf(path,32768,L"%ls/dialog.bmp",directory);char* name=utf8(path);
        if(bytes && name) { memcpy(bytes,&header,sizeof(header));memcpy(bytes+sizeof(header),&info,sizeof(info));memcpy(bytes+header.bfOffBits,pixels,size);if(!create(name,bytes,header.bfSize))okay=0; } else okay=0;
        free(bytes);free(name);
    }
    free(pixels);free(second);CloseHandle(process);return okay;
}
static int capture(HWND window,const wchar_t* directory)
{
    if(GetPropW(window,L"XlangFailureText"))return renderedCapture(window,directory);
    RECT rect;if(!GetWindowRect(window,&rect))return 0;
    int width=rect.right-rect.left,height=rect.bottom-rect.top;if(width<100 || height<100 || width>4000 || height>4000)return 0;
    HDC source=GetDC(window),dc=source ? CreateCompatibleDC(source) : NULL;BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=NULL;HBITMAP bitmap=dc ? CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0) : NULL;
    HGDIOBJ previous=bitmap ? SelectObject(dc,bitmap) : NULL;int okay=0;
    /* An inactive desktop may expose the caption before its dialog body is
       painted. Retry the native window print and verify the actual static text region,
       rather than accepting contrast in the non-client caption alone. */
    for(int attempt=0;bitmap && attempt<25 && !okay;++attempt) {
        memset(pixels,0,(size_t)width*(size_t)height*4);
        RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_FRAME);
        int printed=PrintWindow(window,dc,0);
        GdiFlush();okay=printed && visibleText(window,&rect,pixels,width,height);
        if(!okay)Sleep(10);
    }
    if(bitmap) {
        size_t size=(size_t)width*(size_t)height*4;BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=(DWORD)(header.bfOffBits+size);
        unsigned char* bytes=malloc(header.bfSize);wchar_t path[32768];swprintf(path,32768,L"%ls/dialog.bmp",directory);char* name=utf8(path);
        int saved=bytes && name;
        if(saved) { memcpy(bytes,&header,sizeof(header));memcpy(bytes+sizeof(header),&info.bmiHeader,sizeof(info.bmiHeader));memcpy(bytes+header.bfOffBits,pixels,size);saved=create(name,bytes,header.bfSize); }
        if(!saved)okay=0;
        free(name);free(bytes);
    }
    if(previous)SelectObject(dc,previous);
    if(bitmap)DeleteObject(bitmap);
    if(dc)DeleteDC(dc);
    if(source)ReleaseDC(window,source);
    return okay;
}
static BOOL CALLBACK visit(HWND window,LPARAM user)
{
    launch_scan* scan=(launch_scan*)user;DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=scan->pid)return TRUE;
    wchar_t type[100];GetClassNameW(window,type,100);
    const void* body=GetPropW(window,L"XlangFailureText");
    if(!body && wcscmp(type,L"XlangFailureEmergency"))return TRUE;
    if(scan->dialogs) { PostMessageW(window,WM_CLOSE,0,0);return TRUE; }
    if(!IsWindowVisible(window))return TRUE;
    wchar_t title[200],text[40000];GetWindowTextW(window,title,200);HWND label=GetDlgItem(window,65535);DWORD_PTR copied=0;
    if(body) {
        HANDLE process=OpenProcess(PROCESS_VM_READ,FALSE,pid);SIZE_T size=0;
        int okay=process && ReadProcessMemory(process,body,text,sizeof(text),&size);
        if(process)CloseHandle(process);
        if(!okay || size!=sizeof(text)) { scan->failed=1;return TRUE; }
        text[39999]=0;copied=wcslen(text);
    } else if(label) SendMessageTimeoutW(label,WM_GETTEXT,40000,(LPARAM)text,SMTO_ABORTIFHUNG,2000,&copied);
    if(!copied || !wcsstr(title,L"xlang IDE")) { scan->failed=1;return TRUE; }
    char* bytes=utf8(text);wchar_t path[32768];swprintf(path,32768,L"%ls/dialog.txt",scan->directory);char* name=utf8(path);
    if(!bytes || !name || !create(name,bytes,strlen(bytes)))scan->failed=1;
    free(name);free(bytes);
    if(!capture(window,scan->directory))scan->failed=1;
    ++scan->dialogs;PostMessageW(window,WM_CLOSE,0,0);return TRUE;
}
static int launch(const wchar_t* executable,const wchar_t* directory,const wchar_t* mode)
{
    wchar_t desktopName[100],command[32768],outPath[32768],errPath[32768];swprintf(desktopName,100,L"XlangRuntimeFailure-%lu",GetCurrentProcessId());
    HDESK desktop=CreateDesktopW(desktopName,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(desktop);
    int arguments=!wcscmp(mode,L"arguments"),hidden=!wcscmp(mode,L"hidden"),console=!wcscmp(mode,L"console"),session=!wcscmp(mode,L"session"),defaults=!wcscmp(mode,L"default"),fallback=!wcscmp(mode,L"fallback"),quota=!wcscmp(mode,L"quota");
    const wchar_t* log=(defaults || fallback) ? L"" : L" --diagnostic-root ";
    wchar_t logArguments[32768];logArguments[0]=0;
    if(*log)swprintf(logArguments,32768,L"%ls\"%ls/诊断 文件 😀\"",log,directory);
    if(arguments)swprintf(command,32768,L"\"%ls\" --not-a-valid-argument%ls",executable,logArguments);
    else if(session)swprintf(command,32768,L"\"%ls\" --session-root \"%ls/sessions\" --workspace \"%ls/缺失 工作空间 😀.xlw\" --settings \"%ls/preferences.json\"%ls",executable,directory,directory,directory,logArguments);
    else swprintf(command,32768,L"\"%ls\" %ls --no-session --workspace \"%ls/缺失 工作空间 😀.xlw\" --settings \"%ls/preferences.json\"%ls",executable,hidden ? L"--hidden --frames 3" : L"",directory,directory,logArguments);
    if(fallback)CHECK(SetEnvironmentVariableW(L"LOCALAPPDATA",directory));
    swprintf(outPath,32768,L"%ls/stdout.log",directory);swprintf(errPath,32768,L"%ls/stderr.log",directory);
    SECURITY_ATTRIBUTES sa={0};sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
    HANDLE out=CreateFileW(outPath,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL),err=CreateFileW(errPath,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL),in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(out!=INVALID_HANDLE_VALUE && err!=INVALID_HANDLE_VALUE && in!=INVALID_HANDLE_VALUE);
    HANDLE job=CreateJobObjectW(NULL,NULL);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit={0};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    CHECK(job && SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)));
    STARTUPINFOW start={0};start.cb=sizeof(start);start.lpDesktop=desktopName;start.dwFlags=STARTF_USESTDHANDLES;start.hStdInput=in;start.hStdOutput=out;start.hStdError=err;PROCESS_INFORMATION process={0};
    CHECK(CreateProcessW(executable,command,NULL,NULL,TRUE,CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,NULL,directory,&start,&process));
    if(!AssignProcessToJobObject(job,process.hProcess)) { TerminateProcess(process.hProcess,3);WaitForSingleObject(process.hProcess,5000);CloseHandle(job);return 1; }
    CHECK(ResumeThread(process.hThread)!=(DWORD)-1);CloseHandle(process.hThread);
    printf("runtime failure owned child: pid=%lu\n",process.dwProcessId);fflush(stdout);
    launch_scan scan={process.dwProcessId,directory,0,0};DWORD began=GetTickCount(),code=0;
    while(WaitForSingleObject(process.hProcess,20)==WAIT_TIMEOUT) {
        EnumDesktopWindows(desktop,visit,(LPARAM)&scan);
        if(GetTickCount()-began>30000) { CloseHandle(job);WaitForSingleObject(process.hProcess,5000);fputs("runtime_failure_test: owned native child timed out\n",stderr);return 1; }
    }
    CHECK(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hProcess);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION account={0};CHECK(QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&account,sizeof(account),NULL));
    CloseHandle(job);CloseHandle(out);CloseHandle(err);CloseHandle(in);CloseDesktop(desktop);
    CHECK(!account.ActiveProcesses && !scan.failed && code==(arguments ? 2u : 1u) && scan.dialogs==(hidden || console ? 0 : 1));
    if(scan.dialogs) {
        wchar_t file[32768];swprintf(file,32768,L"%ls/dialog.txt",directory);char* name=utf8(file);char text[16000];CHECK(name && read(name,text,sizeof(text))>0);free(name);
        CHECK(strstr(text,arguments ? "启动参数无效" : "无法打开指定的工作空间"));
        if(!arguments)CHECK(strstr(text,"未生成新的恢复检查点") || strstr(text,"已关闭会话恢复"));
        CHECK(!strstr(text,"xideWorkspaceOpen"));
        if(!wcscmp(mode,L"blocked") || quota)CHECK(strstr(text,"诊断文件未能写入"));
        else CHECK(strstr(text,"诊断文件："));
    }
    printf("runtime_failure_test launch passed: %d checks; pid=%lu exit=%lu dialogs=%d owned-live=%lu mode=",checks,scan.pid,code,scan.dialogs,account.ActiveProcesses);
    char* label=utf8(mode);puts(label ? label : "?");free(label);return 0;
}
int main(void)
{
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 1;
    int result=argc==3 && !wcscmp(argv[1],L"--storage") ? storage(argv[2]) : argc==5 && !wcscmp(argv[1],L"--launch") ? launch(argv[2],argv[3],argv[4]) : 2;
    LocalFree(argv);return result;
}
