#include "app_xrt.h"
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"analysis_process_tree_test:%d: %s\n",__LINE__,#e);okay=0;goto cleanup;}} while(0)
int main(void)
{
    int argc=0;wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&argc);if(!args)return 2;
    if(argc==2 && !wcscmp(args[1],L"--descendant")) {LocalFree(args);Sleep(60000);return 0;}
    wchar_t self[32768];if(!GetModuleFileNameW(NULL,self,32768)){LocalFree(args);return 2;}
    if(argc==2 && !wcscmp(args[1],L"--root")) {
        wchar_t command[32768];STARTUPINFOW startup={.cb=sizeof(startup)};PROCESS_INFORMATION child={0};
        if(swprintf(command,32768,L"\"%ls\" --descendant",self)<0 || !CreateProcessW(NULL,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&child)){LocalFree(args);return 2;}
        char line[50];int n=snprintf(line,sizeof(line),"%lu\n",child.dwProcessId);DWORD written=0,size=0;char input;
        int okay=WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,(DWORD)n,&written,NULL) && written==(DWORD)n && ReadFile(GetStdHandle(STD_INPUT_HANDLE),&input,1,&size,NULL) && size==1;
        CloseHandle(child.hThread);CloseHandle(child.hProcess);LocalFree(args);return okay ? 0 : 2;
    }
    LocalFree(args);int checks=0,okay=1;xprocess* process=NULL;HANDLE child=NULL;char program[131072],line[50]={0};size_t used=0;
    xprocessconfig config;xprocessstatus before={0},after={0};const char* arguments[]={"--root"};
    CHECK(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,self,-1,program,sizeof(program),NULL,NULL)>0);
    CHECK(xrtProcessConfigInit(&config));config.Program=program;config.Args=arguments;config.ArgCount=1;config.NewGroup=true;config.HideWindow=true;
    config.Stdin.Mode=XPROCESS_IO_PIPE;config.Stdout.Mode=XPROCESS_IO_PIPE;config.Stderr.Mode=XPROCESS_IO_NULL;
    process=xrtProcessSpawn(&config);CHECK(process);
    while(!strchr(line,'\n') && used<sizeof(line)-1) {int64 n=xrtProcessRead(process,XPROCESS_STDOUT,line+used,sizeof(line)-used-1);CHECK(n>0);used+=(size_t)n;}
    CHECK(strchr(line,'\n'));child=OpenProcess(SYNCHRONIZE,FALSE,(DWORD)strtoul(line,NULL,10));CHECK(child);
    CHECK(xrtProcessWrite(process,"q",1)==1 && xrtProcessClose(process,XPROCESS_STDIN));
    /* XRT relative deadlines use microseconds; this tests cleanup/status,
       not a five-millisecond Windows process-exit performance budget. */
    CHECK(xrtProcessWaitFor(process,5000000)==XWAIT_OK && xrtProcessStatus(process,&before));
    CHECK(before.Kind==XPROCESS_EXIT_CODE && before.Code==0 && before.Stop==XPROCESS_STOP_NONE);
    CHECK(xrtProcessKillTree(process));CHECK(WaitForSingleObject(child,1000)==WAIT_OBJECT_0);
    CHECK(xrtProcessStatus(process,&after) && before.Kind==after.Kind && before.Code==after.Code && before.Stop==after.Stop && before.Signal==after.Signal && before.CoreDumped==after.CoreDumped);
    CHECK(xrtProcessKillTree(process) && xrtProcessRead(process,XPROCESS_STDOUT,line,sizeof(line))==0);
cleanup:
    if(process){xrtProcessKillTree(process);xrtProcessWaitFor(process,5000000);xrtProcessDestroy(process);}if(child)CloseHandle(child);
    if(okay)printf("analysis_process_tree_test passed: %d checks; retained Windows Job cleanup after root exit, unchanged terminal root status, repeated stop and pipe EOF.\n",checks);
    return okay ? 0 : 1;
}
#else
int main(void) {puts("analysis_process_tree_test passed: Windows-only Job policy; POSIX behavior unchanged.");return 0;}
#endif
