/* 前台服务辅助：进程身份、停止请求与排他 PID 文件。 */
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_FILE_LOCK
#define XRT_MODULE_FILE_LOCK
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/daemon_native.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#endif

typedef struct xdaemon {
    str name;
    str pid_path;
    xfile pid_file;
    long long pid;
    xatomic32 stop_requested;
} xdaemon;

/* Windows 的排他锁会拒绝读取锁定字节；锁定数据区外的单个字节。 */
#define XDAEMON_LOCK_OFFSET 4096u

static int xdaemon_lock(xfile file)
{
#ifdef _WIN32
    if (xrtFileLockRange(file, XFILE_LOCK_EXCLUSIVE,
        XDAEMON_LOCK_OFFSET, 1u, false)) return 1;
    if (xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_AGAIN) xrtClearError();
    return 0;
#else
    int descriptor = (int)xrtFileNative(file);
    if (descriptor < 0) return 0;
    if (flock(descriptor, LOCK_EX | LOCK_NB) == 0) return 1;
    if (errno != EWOULDBLOCK && errno != EAGAIN)
        xrtSetErrorInfo(XERR_IO, "xlang.extlib.daemon", errno,
            "failed to lock PID file");
    return 0;
#endif
}

static int xdaemon_unlock(xfile file)
{
#ifdef _WIN32
    return xrtFileUnlockRange(file, XDAEMON_LOCK_OFFSET, 1u);
#else
    int descriptor = (int)xrtFileNative(file);
    if (descriptor < 0) return 0;
    if (flock(descriptor, LOCK_UN) == 0) return 1;
    xrtSetErrorInfo(XERR_IO, "xlang.extlib.daemon", errno,
        "failed to unlock PID file");
    return 0;
#endif
}

static int xdaemon_text_valid(const char *text, size_t limit)
{
    size_t size;
    size_t i;
    if (text == NULL) return 0;
    for (size = 0u; size <= limit && text[size] != '\0'; ++size) {}
    if (size == 0 || size > limit || !xrtUtf8Valid((xstrview){text, size}, NULL)) return 0;
    for (i = 0; i < size; ++i)
        if ((unsigned char)text[i] < 32u || (unsigned char)text[i] == 127u) return 0;
    return 1;
}

void *xdaemon_create(const char *name)
{
    xdaemon *state;
    xrtClearError();
    if (!xdaemon_text_valid(name, 255u)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.daemon", 0,
            "name must be valid UTF-8 without control characters");
        return NULL;
    }
    state = (xdaemon *)xrtCalloc(1u, sizeof(*state));
    if (state == NULL) return NULL;
    state->name = xrtStrDup(name);
    if (state->name == NULL) {
        xrtFree(state);
        return NULL;
    }
    xrtAtomic32Init(&state->stop_requested, 0u);
#ifdef _WIN32
    state->pid = (long long)_getpid();
#else
    state->pid = (long long)getpid();
#endif
    return state;
}

/* 删除的必须是仍由本对象打开的同一文件，不能误删后来替换的路径。 */
static int64_t xdaemon_release_pidfile_state(xdaemon *state)
{
    int okay = 1;
    if (state == NULL) return 0;
    if (state->pid_file == NULL) return 1;
    if (!xrtFileDeleteOpen(state->pid_file, state->pid_path)) okay = 0;
    if (!xdaemon_unlock(state->pid_file)) okay = 0;
    if (!xrtClose(state->pid_file)) okay = 0;
    state->pid_file = NULL;
    xrtFree(state->pid_path);
    state->pid_path = NULL;
    return okay;
}

int64_t xdaemon_release_pidfile(void *handle)
{
    xrtClearError();
    return xdaemon_release_pidfile_state((xdaemon *)handle);
}

void xdaemon_destroy(void *handle)
{
    xdaemon *state = (xdaemon *)handle;
    if (state == NULL) return;
    xdaemon_release_pidfile_state(state);
    xrtFree(state->name);
    xrtFree(state);
}

int64_t xdaemon_write_pidfile(void *handle, const char *path)
{
    xdaemon *state = (xdaemon *)handle;
    xfileoptions options;
    xfile file;
    str copy;
    str content;
    size_t size;
    int okay;

    xrtClearError();
    if (state == NULL || !xdaemon_text_valid(path, 4096u)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.daemon", 0,
            "PID path must be valid UTF-8 without control characters");
        return 0;
    }
    if (state->pid_path != NULL) {
        if (strcmp(state->pid_path, path) == 0) return 1;
        xrtSetErrorInfo(XERR_STATE, "xlang.extlib.daemon", 0,
            "release the current PID file before changing its path");
        return 0;
    }

    xrtFileOptionsInit(&options);
    options.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_NOFOLLOW;
    options.Mode = 0600u;
    file = xrtFileOpen(path, &options);
    if (file == NULL) return 0;
    if (!xdaemon_lock(file)) {
        xrtClose(file);
        return 0;
    }
    copy = xrtStrDup(path);
    content = xrtFormat("%lld\n", state->pid);
    if (copy == NULL || content == NULL) {
        xrtFree(copy);
        xrtFree(content);
        xdaemon_unlock(file);
        xrtClose(file);
        return 0;
    }
    size = strlen(content);
    okay = xrtFileResize(file, 0u) &&
        xrtWriteAtFull(file, 0u, content, size, NULL) && xrtFlush(file);
    xrtFree(content);
    if (!okay) {
        xrtFree(copy);
        xdaemon_unlock(file);
        xrtClose(file);
        return 0;
    }
    state->pid_file = file;
    state->pid_path = copy;
    return 1;
}

int64_t xdaemon_read_pidfile(const char *path)
{
    xfile file;
    char data[64];
    char *end;
    uint64 size;
    int64_t pid;
    xrtClearError();
    if (!xdaemon_text_valid(path, 4096u)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.daemon", 0,
            "PID path must be valid UTF-8 without control characters");
        return -1;
    }
    file = xrtOpen(path, XFILE_READ | XFILE_NOFOLLOW);
    if (file == NULL) {
        if (xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) xrtClearError();
        return -1;
    }
    if (!xrtFileSize(file, &size) || size == 0u || size >= sizeof(data) ||
        !xrtReadFull(file, data, (size_t)size, NULL)) {
        xerror *failure = xrtTakeError();
        (void)xrtClose(file);
        if (failure != NULL) xrtSetErrorTake(failure);
        return -1;
    }
    if (!xrtClose(file)) return -1;
    if (memchr(data, '\0', (size_t)size) != NULL) return -1;
    data[size] = '\0';
    if (size == 0u || data[0] < '0' || data[0] > '9') return -1;
    errno = 0;
    pid = strtoll(data, &end, 10);
    if (errno == ERANGE || pid <= 0 || (end[0] != '\0' &&
        !(end[0] == '\n' && end[1] == '\0'))) return -1;
    return pid;
}

int64_t xdaemon_is_alive(int64_t pid)
{
    xrtClearError();
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE process;
    DWORD code;
    if ((unsigned long long)pid > UINT_MAX) return 0;
    process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (process == NULL) return GetLastError() == ERROR_ACCESS_DENIED;
    if (!GetExitCodeProcess(process, &code)) {
        CloseHandle(process);
        return 0;
    }
    CloseHandle(process);
    return code == STILL_ACTIVE;
#else
    if (pid > INT_MAX) return 0;
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno == EPERM;
#endif
}

int64_t xdaemon_get_pid(void *handle) { xdaemon *state = (xdaemon *)handle; return state ? state->pid : -1; }
const char *xdaemon_name(void *handle) { xdaemon *state = (xdaemon *)handle; return state ? state->name : ""; }
int64_t xdaemon_stop_requested(void *handle) { xdaemon *state = (xdaemon *)handle; return state ? xrtAtomic32Load(&state->stop_requested, XMEMORY_ACQUIRE) : 0; }
void xdaemon_request_stop(void *handle) { xdaemon *state = (xdaemon *)handle; if (state) xrtAtomic32Store(&state->stop_requested, 1u, XMEMORY_RELEASE); }
int64_t xdaemon_holds_pidfile(void *handle) { xdaemon *state = (xdaemon *)handle; return state && state->pid_file != NULL; }
