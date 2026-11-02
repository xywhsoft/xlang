#include "xrt.h"
#include "../include/singleinstance_native.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "singleinstance test failed at line %d: %s\n", __LINE__, #test); \
    if (xrtGetError() != NULL) \
        fprintf(stderr, "XRT error: %s (system=%d)\n", \
            xrtErrorMessage(xrtGetError()), xrtErrorSystemCode(xrtGetError())); \
    return 1; \
} } while (0)

static int process_check(const char* name, int expect_acquired)
{
    int result = xsi_acquire(name);
    /* 持有分支故意不 release，验证进程退出自动解锁。 */
    return result == expect_acquired ? 0 : 1;
}

static int run_child(const char* self, const char* name, int expected, int unicode)
{
#ifdef _WIN32
    wchar_t path[512], wide_name[160], command[768];
    const wchar_t* base;
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD code = 1;
    (void)self;
    DWORD length=GetModuleFileNameW(NULL,path,512);
    if (!length || length>=512) return 1;
    base = wcsrchr(path, L'\\');
    if (base != NULL && _wcsicmp(base + 1, L"xl.exe") == 0) return 2;
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,wide_name,160);
    if(count<=0)return 1;
    /* 测试只传入固定模式、生成的无引号名称和单个0/1参数。 */
    const wchar_t* middle=unicode ? L"\" --child-unicode \"" : L"\" --child \"";
    size_t used=1u+length+wcslen(middle)+(size_t)count-1u;
    if(used+5u>sizeof(command)/sizeof(command[0]))return 1;
    command[0]=L'"';memcpy(command+1,path,length*sizeof(wchar_t));
    memcpy(command+1+length,middle,wcslen(middle)*sizeof(wchar_t));
    memcpy(command+1+length+wcslen(middle),wide_name,(size_t)(count-1)*sizeof(wchar_t));
    command[used++]=L'"';command[used++]=L' ';command[used++]=expected ? L'1' : L'0';command[used]=0;
    startup.cb = sizeof(startup);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
        &startup, &process)) return 1;
    DWORD waited=WaitForSingleObject(process.hProcess,20000);
    if(waited!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,10000);}
    else if(!GetExitCodeProcess(process.hProcess,&code))code=1;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return waited==WAIT_OBJECT_0 && code==0 ? 0 : 1;
#else
    pid_t pid = fork();
    int status;
    char want[4];
    if (pid < 0) return 1;
    if (pid == 0) {
        snprintf(want, sizeof(want), "%d", expected);
        execl(self, self, unicode ? "--child-unicode" : "--child",
            name, want, (char*)NULL);
        _exit(127);
    }
    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
#endif
}

int main(int argc, char** argv)
{
    char name[96];
    char unicode_name[128];
    char unterminated_name[129];
    char boundary_name[130];
    size_t name_length;
    int child_status;
    if (argc == 4 && strcmp(argv[1], "--child") == 0)
        return process_check(argv[2], argv[3][0] == '1' ? 1 : 0);
    if (argc == 4 && strcmp(argv[1], "--child-unicode") == 0) {
        char child_name[128];
        if (snprintf(child_name, sizeof(child_name), "中文_%s", argv[2]) >=
            (int)sizeof(child_name)) return 1;
        return process_check(child_name, argv[3][0] == '1' ? 1 : 0);
    }
#ifndef _WIN32
    if (argc == 2 && strcmp(argv[1], "--unsafe-dir") == 0) {
        int result = xsi_acquire("unsafe_directory_probe");
        if (result == 1) xsi_release();
        return result == -1 && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_PERMISSION ? 0 : 1;
    }
#endif
#ifdef _WIN32
    snprintf(name, sizeof(name), "xlang_si_%lu", (unsigned long)GetCurrentProcessId());
#else
    snprintf(name, sizeof(name), "xlang_si_%lu", (unsigned long)getpid());
#endif
    snprintf(unicode_name, sizeof(unicode_name), "中文_%s", name);
    name_length = strlen(name);
    memcpy(boundary_name, name, name_length);
    memset(boundary_name + name_length, 'x', 128u - name_length);
    boundary_name[128] = '\0';
    CHECK(xsi_acquire(boundary_name) == 1);
    CHECK(xsi_release() == 1);
    boundary_name[128] = 'x';
    boundary_name[129] = '\0';
    CHECK(xsi_acquire(boundary_name) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    memset(unterminated_name, 'a', sizeof(unterminated_name));
    CHECK(xsi_acquire(unterminated_name) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xsi_acquire("") == -1);
    CHECK(xrtGetError() != NULL);
    CHECK(!xsi_is_acquired() && xrtGetError() == NULL);
    CHECK(xsi_acquire(name) == 1);
    CHECK(xsi_is_acquired());
    CHECK(xsi_acquire(name) == -1);
    CHECK(xrtGetError() != NULL);
    child_status = run_child(argv[0], name, 0, 0);
    CHECK(child_status == 0 || child_status == 2);
    CHECK(xsi_release() == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(!xsi_is_acquired());
    CHECK(xsi_release() == 1);
    if (child_status != 2) CHECK(run_child(argv[0], name, 1, 0) == 0);
    CHECK(xsi_acquire(name) == 1);
    CHECK(xsi_release() == 1);
    CHECK(xsi_acquire(unicode_name) == 1);
    if (child_status != 2) CHECK(run_child(argv[0], name, 0, 1) == 0);
    CHECK(xsi_release() == 1);
    if (child_status != 2) CHECK(run_child(argv[0], name, 1, 1) == 0);
    puts(child_status==2 ? "singleinstance cross-process: not run inside xl.exe host" : "singleinstance cross-process: real child");
    puts("singleinstance C boundaries: PASS");
    return 0;
}
