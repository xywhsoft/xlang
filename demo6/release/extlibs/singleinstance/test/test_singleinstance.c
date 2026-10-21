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
    if (!GetModuleFileNameW(NULL, path, 512)) return 1;
    base = wcsrchr(path, L'\\');
    if (base != NULL && _wcsicmp(base + 1, L"xl.exe") == 0) return 2;
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 160)) return 1;
    if (swprintf(command, 768, L"\"%ls\" %ls %ls %d",
        path, unicode ? L"--child-unicode" : L"--child", wide_name,
        expected) < 0) return 1;
    startup.cb = sizeof(startup);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, NULL,
        &startup, &process)) return 1;
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0 ? 0 : 1;
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
    puts("singleinstance C boundaries: PASS");
    return 0;
}
