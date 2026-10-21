/* 进程级单实例锁；锁文件保留名称，进程退出由操作系统释放文件锁。 */
#ifndef XRT_MODULE_FILE_LOCK
#define XRT_MODULE_FILE_LOCK
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#include "xrt.h"
#include "include/singleinstance_native.h"
#include <string.h>
#ifndef _WIN32
#include <errno.h>
#include <sys/file.h>
#endif

static xonce xsi_once = XRT_ONCE_INIT;
static xmutex xsi_guard;
static xfile xsi_file;

static bool xsi_init(ptr unused)
{
    (void)unused;
    return xrtMutexInit(&xsi_guard);
}

static bool xsi_enter(void)
{
    return xrtOnce(&xsi_once, xsi_init, NULL) && xrtMutexLock(&xsi_guard);
}

static void xsi_leave(void)
{
    xrtMutexUnlock(&xsi_guard);
}

static bool xsi_name_valid(const char* name)
{
    size_t length;
    size_t i;
    if (name == NULL) return false;
    for (length = 0u; length <= 128u && name[length] != '\0'; ++length) {}
    if (length == 0u || length > 128u ||
        !xrtUtf8Valid((xstrview){name, length}, NULL)) return false;
    for (i = 0u; i < length; ++i)
        if ((unsigned char)name[i] < 32u || (unsigned char)name[i] == 127u)
            return false;
    return true;
}

static bool xsi_private_directory(const char* path)
{
    xfileinfo info;
    if (!xrtPathStat(path, false, &info)) return false;
    if (info.Type == XFILE_TYPE_DIRECTORY
#ifndef _WIN32
        && (info.Available & XFILE_INFO_MODE) != 0u &&
        (info.Mode & 0077u) == 0u
#endif
    ) return true;
    xrtSetErrorInfo(XERR_PERMISSION, "xlang.extlib.singleinstance", 0,
        "lock directory must be a private real directory");
    return false;
}

/* 返回 1=持有，0=另一包实例持有，-1=输入/IO 错误。 */
int xsi_acquire(const char* name)
{
    const xbase64config config = {NULL, XBASE64_URL | XBASE64_NO_PADDING};
    xfileoptions options;
    str home = NULL, root = NULL, directory = NULL, encoded = NULL, path = NULL;
    xfile file = NULL;
    int result = -1;

    if (!xsi_name_valid(name)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.singleinstance", 0,
            "name must be 1..128 valid UTF-8 bytes without control characters");
        return -1;
    }
    xrtClearError();
    if (!xsi_enter()) return -1;
    if (xsi_file != NULL) {
        xrtSetErrorInfo(XERR_STATE, "xlang.extlib.singleinstance", 0,
            "this process already holds an instance lock");
        goto done;
    }
    home = xrtPathHome();
    if (home == NULL) goto done;
    root = xrtPathJoin(home, ".xlang");
    if (root == NULL || !xrtDirCreateAllMode(root, 0700u) ||
        !xsi_private_directory(root)) goto done;
    directory = xrtPathJoin(root, "singleinstance");
    if (directory == NULL || !xrtDirCreateAllMode(directory, 0700u) ||
        !xsi_private_directory(directory)) goto done;
    encoded = xrtBase64EncodeNew(name, strlen(name), &config);
    if (encoded == NULL) goto done;
    path = xrtPathJoin(directory, encoded);
    if (path == NULL) goto done;

    xrtFileOptionsInit(&options);
    options.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_NOFOLLOW;
    options.Mode = 0600u;
    file = xrtFileOpen(path, &options);
    if (file == NULL) goto done;
#ifdef _WIN32
    if (!xrtFileLock(file, XFILE_LOCK_EXCLUSIVE, false)) {
        if (xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_AGAIN) {
            xrtClearError();
            result = 0;
        }
        goto done;
    }
#else
    /* fcntl 锁按进程计；flock 按打开句柄计，两个 JIT 上下文才会互斥。 */
    {
        intptr_t native = xrtFileNative(file);
        int status;
        if (native < 0) goto done;
        do { status = flock((int)native, LOCK_EX | LOCK_NB); }
        while (status != 0 && errno == EINTR);
        if (status != 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                xrtClearError();
                result = 0;
            } else {
                xrtSetErrorInfo(XERR_IO, "xlang.extlib.singleinstance", errno,
                    "could not lock instance file");
            }
            goto done;
        }
    }
#endif
    xsi_file = file;
    file = NULL;
    result = 1;

done:
    if (file != NULL) xrtClose(file);
    xrtFree(path);
    xrtFree(encoded);
    xrtFree(directory);
    xrtFree(root);
    xrtFree(home);
    xsi_leave();
    return result;
}

int xsi_release(void)
{
    int success = 1;
    xrtClearError();
    if (!xsi_enter()) return 0;
    if (xsi_file != NULL) {
#ifdef _WIN32
        success = xrtFileUnlock(xsi_file) ? 1 : 0;
#else
        if (flock((int)xrtFileNative(xsi_file), LOCK_UN) != 0) {
            xrtSetErrorInfo(XERR_IO, "xlang.extlib.singleinstance", errno,
                "could not unlock instance file");
            success = 0;
        }
#endif
        if (!xrtClose(xsi_file)) success = 0;
        xsi_file = NULL;
    }
    xsi_leave();
    return success;
}

int xsi_is_acquired(void)
{
    int acquired;
    xrtClearError();
    if (!xsi_enter()) return 0;
    acquired = xsi_file != NULL;
    xsi_leave();
    return acquired;
}
