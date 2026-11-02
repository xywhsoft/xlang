/* 保留已退出进程的原生句柄，避免 PID 重用干扰 Windows 259 回归。 */
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#include "../daemon_native.c"
#include <stdio.h>
#ifdef _WIN32
#include <wchar.h>
#else
#include <sys/wait.h>
#endif

static unsigned checks, failures;
static void require(int ok, const char* label)
{
    ++checks;
    if (!ok) { fprintf(stderr, "daemon liveness: %s\n", label); ++failures; }
}
int main(void)
{
    void* service = xdaemon_create("liveness-contract");
    if (!service) return 2;
    require(xdaemon_is_alive(xdaemon_get_pid(service)) == 1, "current process alive");
    require(xdaemon_is_alive(0) == 0 && xdaemon_is_alive(-1) == 0 && xdaemon_is_alive(INT64_MAX) == 0, "invalid pid is not alive");
#ifdef _WIN32
    static const wchar_t* exits[] = {L"0",L"1",L"2",L"258",L"259",L"260",L"2147483647",L"4294967295"};
    static const DWORD expected[] = {0,1,2,258,259,260,2147483647u,4294967295u};
    for (unsigned i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i) {
        wchar_t system[32768], command[32768];
        UINT length = GetSystemDirectoryW(system, 32768);
        if (!length || length+64 >= 32768) return 2;
        wcscpy(command,L"\""); wcscat(command,system); wcscat(command,L"\\cmd.exe\" /d /c exit "); wcscat(command,exits[i]);
        STARTUPINFOW start; PROCESS_INFORMATION child;
        memset(&start,0,sizeof(start)); memset(&child,0,sizeof(child)); start.cb=sizeof(start);
        if (!CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&start,&child)) return 2;
        DWORD waited=WaitForSingleObject(child.hProcess,10000), code=0;
        require(waited == WAIT_OBJECT_0, "owned child completed within bound");
        if (waited != WAIT_OBJECT_0) { TerminateProcess(child.hProcess,1); WaitForSingleObject(child.hProcess,10000); }
        require(GetExitCodeProcess(child.hProcess,&code) && code==expected[i], "kernel confirms real child exit code");
        DWORD before=0,after=0;
        BOOL measured=GetProcessHandleCount(GetCurrentProcess(),&before);
        int64_t alive=xdaemon_is_alive(child.dwProcessId);
        measured=measured && GetProcessHandleCount(GetCurrentProcess(),&after);
        require(alive == 0, "retired child is not alive even for exit code 259");
        require(measured && before==after, "isAlive closes its own process handle");
        CloseHandle(child.hThread); CloseHandle(child.hProcess);
    }
#else
    static const int exits[] = {0,1,2,3,255};
    for (unsigned i=0; i<sizeof(exits)/sizeof(exits[0]); ++i) {
        pid_t child=fork(); int status=0;
        if (child < 0) return 2;
        if (child == 0) _exit(exits[i]);
        require(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==exits[i], "reap own exited child");
        require(xdaemon_is_alive(child)==0, "reaped child is not alive");
    }
#endif
    xdaemon_destroy(service);
    printf("daemon liveness: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
