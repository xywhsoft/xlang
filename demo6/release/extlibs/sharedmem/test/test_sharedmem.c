#define XRT_MODULE_BUFFER
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
#include "../include/sharedmem_native.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "sharedmem test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int child(const char* name)
{
    void* shared = xshm_open(name);
    int64_t value = 0;
    if (shared == NULL || !xshm_read_int64(shared, 16, &value) || value != -123456789) {
        xshm_close(shared);
        return 1;
    }
    if (!xshm_write(shared, 40, "child", 5)) {
        xshm_close(shared);
        return 1;
    }
    xshm_close(shared);
    return 0;
}

static int test_process(const char* name)
{
#ifdef _WIN32
    wchar_t self[512], wide_name[160], command[768];
    const wchar_t* base;
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD code = 1;
    if (!GetModuleFileNameW(NULL, self, 512)) return 1;
    base = wcsrchr(self, L'\\');
    if (base != NULL && _wcsicmp(base + 1, L"xl.exe") == 0) return 2;
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 160)) return 1;
    if (swprintf(command, 768, L"\"%ls\" --child %ls", self, wide_name) < 0) return 1;
    startup.cb = sizeof(startup);
    if (!CreateProcessW(self, command, NULL, NULL, FALSE, 0, NULL, NULL,
        &startup, &process)) return 1;
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0 ? 0 : 1;
#else
    pid_t pid = fork();
    int status;
    if (pid < 0) return 1;
    if (pid == 0) _exit(child(name));
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
    uint64_t nonce;
    void* owner;
    void* peer;
    xbuffer* copy;
    xbytesview view;
    int64_t value;
    int process_result;
    const unsigned char binary[] = {65, 0, 255};
    if (argc == 3 && strcmp(argv[1], "--child") == 0) return child(argv[2]);
    CHECK(xrtSecureRandom(&nonce, sizeof(nonce)));
#ifdef _WIN32
    snprintf(name, sizeof(name), "xlang_shm_c_%lu_%016llx",
        (unsigned long)GetCurrentProcessId(), (unsigned long long)nonce);
#else
    snprintf(name, sizeof(name), "xlang_shm_c_%lu_%016llx",
        (unsigned long)getpid(), (unsigned long long)nonce);
#endif
    snprintf(unicode_name, sizeof(unicode_name), "中文_%s", name);
    name_length = strlen(name);
    memcpy(boundary_name, name, name_length);
    memset(boundary_name + name_length, 'x', 128u - name_length);
    boundary_name[128] = '\0';
    owner = xshm_create(boundary_name, 64);
    CHECK(owner != NULL);
    peer = xshm_open(boundary_name);
    CHECK(peer != NULL);
    xshm_close(peer);
    xshm_close(owner);
    boundary_name[128] = 'x';
    boundary_name[129] = '\0';
    CHECK(xshm_create(boundary_name, 64) == NULL);
    CHECK(xshm_open(boundary_name) == NULL);
    CHECK(!xshm_unlink(boundary_name));
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    memset(unterminated_name, 'a', sizeof(unterminated_name));
    CHECK(xshm_create(unterminated_name, 64) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xshm_create("", 64) == NULL);
    CHECK(xshm_create("bad/name", 64) == NULL);
    CHECK(xshm_create(name, 0) == NULL);
    CHECK(xshm_create(name, 67108865) == NULL);
    owner = xshm_create(name, 64);
    CHECK(owner != NULL && xshm_size(owner) == 64);
    CHECK(xshm_size(NULL) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xshm_size(owner) == 64 && xrtGetError() == NULL);
    CHECK(strcmp(xshm_name_of(NULL), "") == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(strcmp(xshm_name_of(owner), name) == 0 && xrtGetError() == NULL);
    CHECK(xshm_create(name, 64) == NULL);
    peer = xshm_open(name);
    CHECK(peer != NULL && strcmp(xshm_name_of(peer), name) == 0);

    CHECK(xshm_write(owner, 0, binary, sizeof(binary)));
    copy = (xbuffer*)xshm_read(peer, 0, 3);
    CHECK(copy != NULL);
    view = xrtBufferView(copy);
    CHECK(view.Size == 3 && memcmp(view.Data, binary, 3) == 0);
    xrtBufferDestroy(copy);
    CHECK(!xshm_write(owner, -1, binary, 1));
    CHECK(!xshm_write(owner, 63, binary, 3));
    CHECK(!xshm_write(owner, 0, NULL, 1));
    CHECK(xshm_read(owner, 65, 0) == NULL);
    CHECK(xshm_read_int64(owner, 0, NULL) == 0);
    CHECK(xrtGetError() != NULL);

    CHECK(xshm_write_int64(peer, 16, -123456789));
    CHECK(xrtGetError() == NULL);
    CHECK(xshm_read_int64(owner, 16, &value) && value == -123456789);
    process_result = test_process(name);
    CHECK(process_result == 0 || process_result == 2);
#ifdef _WIN32
    /* xl run 下的 C 测试无法自启动；独立 GCC 可执行文件覆盖此路径。 */
    if (process_result == 2) CHECK(xshm_write(peer, 40, "child", 5));
#endif
    copy = (xbuffer*)xshm_read(owner, 40, 5);
    CHECK(copy != NULL);
    view = xrtBufferView(copy);
    CHECK(view.Size == 5 && memcmp(view.Data, "child", 5) == 0);
    xrtBufferDestroy(copy);
    CHECK(xshm_zero(owner));
    CHECK(xshm_read_int64(peer, 16, &value) && value == 0);
    xshm_close(peer);
    xshm_close(owner);
    CHECK(xshm_open(name) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    owner = xshm_create(unicode_name, 32);
    CHECK(owner != NULL);
    peer = xshm_open(unicode_name);
    CHECK(peer != NULL && xshm_write(owner, 0, "utf8", 4));
    xshm_close(peer);
    xshm_close(owner);
#ifndef _WIN32
    owner = xshm_create(name, 16);
    CHECK(owner != NULL && xshm_unlink(name));
    CHECK(xshm_open(name) == NULL);
    xshm_close(owner);
    {
        char reused_name[128];
        int survived;
        snprintf(reused_name, sizeof(reused_name), "%s_reused", name);
        owner = xshm_create(reused_name, 16);
        CHECK(owner != NULL && xshm_unlink(reused_name));
        peer = xshm_create(reused_name, 16);
        CHECK(peer != NULL);
        xshm_close(owner);
        owner = xshm_open(reused_name);
        survived = owner != NULL;
        xshm_close(owner);
        xshm_close(peer);
        CHECK(survived);
    }
#endif
    puts("sharedmem C boundaries: PASS");
    return 0;
}
