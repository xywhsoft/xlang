#include "platform/path.h"
#include <xge.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char* utf8(const wchar_t* text)
{
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);
    char* p=n ? malloc((size_t)n) : NULL;
    if(p && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,p,n,NULL,NULL)) { free(p); p=NULL; }
    return p;
}
int main(void)
{
    int count=0,code=2; wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&count);
    char *program=NULL,*base=NULL,*raw=NULL,*result=NULL; xprocess* process=NULL;
    if(!args || count!=4) goto done;
    program=utf8(args[0]); base=utf8(args[2]); raw=utf8(args[3]);
    if(!program || !base || !raw) goto done;
    if(!wcscmp(args[1],L"ide")) result=xidePathResolveLexical(base,raw);
    else if(!wcscmp(args[1],L"canonical")) result=xidePathResolve(base,raw);
    else if(!wcscmp(args[1],L"win32")) {
        wchar_t full[32768]; DWORD n=GetFullPathNameW(args[3],32768,full,NULL);
        if(n && n<32768) result=utf8(full);
    } else if(!wcscmp(args[1],L"sdk")) {
        /* Exercise the fixed DLL's actual custom inherited environment block,
           as used for captured XLANG_PACKAGE_PATH in the task service. */
        const char* arguments[]={"win32",base,raw}; xprocessenv env={"XIDE_PATH_CONTEXT_TEST","1"};
        xprocessconfig config; xrtProcessConfigInit(&config); config.Program=program;
        config.Args=arguments; config.ArgCount=3; config.WorkDir=base; config.Env=&env; config.EnvCount=1;
        config.HideWindow=true; config.Stdin.Mode=XPROCESS_IO_NULL; config.Stdout.Mode=XPROCESS_IO_PIPE; config.Stderr.Mode=XPROCESS_IO_MERGE;
        process=xrtProcessSpawn(&config); if(!process) goto done;
        unsigned char bytes[4096]; int64 n;
        while((n=xrtProcessRead(process,XPROCESS_STDOUT,bytes,sizeof(bytes)))>0) {
            DWORD written=0;
            if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes,(DWORD)n,&written,NULL) || written!=(DWORD)n) goto done;
        }
        xprocessstatus status={0};
        if(n<0 || xrtProcessWaitFor(process,5*XRT_TIME_SECOND)!=XWAIT_OK || !xrtProcessStatus(process,&status)) goto done;
        code=(int)status.Code; goto done;
    }
    if(result) {
        DWORD written=0,size=(DWORD)strlen(result);
        code=WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),result,size,&written,NULL) && written==size ? 0 : 3;
    }
done:
    if(process) xrtProcessDestroy(process);
    free(result); free(program); free(base); free(raw); if(args) LocalFree(args); return code;
}
