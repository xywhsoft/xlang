/* 只替换故障边界；正常打开、锁定、删除、关闭均实际调用 XRT/内核。 */
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <errno.h>
#include <sys/file.h>
#endif

enum { DELETE_FILE, UNLOCK_FILE, CLOSE_FILE, COPY_PATH, FORMAT_PID, RESIZE_FILE, WRITE_FILE, FLUSH_FILE, CALL_COUNT };
static struct {
    unsigned mask, calls[CALL_COUNT];
    bool active, copyFailed;
    xerror* causes[CALL_COUNT];
} fault;
static unsigned cases, checks, failures, oomRuns, oomFaults;

static void require(bool ok, const char* label)
{
    ++checks;
    if (!ok) { if (failures < 12) fprintf(stderr, "daemon cleanup: %s\n", label); ++failures; }
}
static bool fail_at(unsigned call)
{
    if (!fault.active) return false;
    ++fault.calls[call];
    if (!(fault.mask & (1u << call))) return false;
    xrtSetError(fault.causes[call]);
    require(xrtMemDebugFailAfter(0), "guard diagnostic preservation against new allocation");
    return true;
}
static bool delete_file(xfile file, const char* path)
{ return fail_at(DELETE_FILE) ? false : xrtFileDeleteOpen(file, path); }
static bool close_file(xfile file)
{
    /* Close 即使返回失败也消耗句柄；先真正退休，再模拟报告失败。 */
    bool ok = xrtClose(file);
    return fail_at(CLOSE_FILE) ? false : ok;
}
#ifdef _WIN32
static bool unlock_file(xfile file, uint64 offset, uint64 length)
{
    bool ok = xrtFileUnlockRange(file, offset, length);
    return fail_at(UNLOCK_FILE) ? false : ok;
}
#else
static int probe_flock(int descriptor, int operation)
{
    int result = flock(descriptor, operation);
    /* POSIX flock 不发布XRT错误，由生产 xdaemon_unlock 按 errno 构造。 */
    if (operation == LOCK_UN && fault.active) {
        ++fault.calls[UNLOCK_FILE];
        if (fault.mask & (1u << UNLOCK_FILE)) { errno = EIO; return -1; }
    }
    return result;
}
#endif
static char* copy_path(const char* path)
{
    if (fail_at(COPY_PATH)) { fault.copyFailed = true; return NULL; }
    char* result = xrtStrDup(path);
    if (fault.active && result == NULL) fault.copyFailed = true;
    return result;
}
static char* format_pid(const char* format, long long pid)
{ return fail_at(FORMAT_PID) ? NULL : xrtFormat(format, pid); }
static bool resize_file(xfile file, uint64 size)
{ return fail_at(RESIZE_FILE) ? false : xrtFileResize(file, size); }
static bool write_file(xfile file, uint64 offset, const void* data, size_t size, size_t* written)
{ return fail_at(WRITE_FILE) ? false : xrtWriteAtFull(file, offset, data, size, written); }
static bool flush_file(xfile file)
{ return fail_at(FLUSH_FILE) ? false : xrtFlush(file); }

#define xrtFileDeleteOpen delete_file
#define xrtClose close_file
#define xrtStrDup copy_path
#define xrtFormat format_pid
#define xrtFileResize resize_file
#define xrtWriteAtFull write_file
#define xrtFlush flush_file
#ifdef _WIN32
#define xrtFileUnlockRange unlock_file
#else
#define flock probe_flock
#endif
#include "../daemon_native.c"
#undef xrtFileDeleteOpen
#undef xrtClose
#undef xrtStrDup
#undef xrtFormat
#undef xrtFileResize
#undef xrtWriteAtFull
#undef xrtFlush
#ifdef _WIN32
#undef xrtFileUnlockRange
#else
#undef flock
#endif

static bool memory_equal(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}
static void reset_faults(unsigned mask, xerrkind kind)
{
    memset(&fault, 0, sizeof(fault)); fault.mask = mask;
    for (unsigned i = 0; i < CALL_COUNT; ++i) {
        xrtSetErrorInfo(kind, "test.daemon.cleanup", -101-(int)i, "original cleanup cause");
        fault.causes[i] = xrtTakeError();
        require(fault.causes[i] != NULL, "own error before fault budget");
    }
}
static void finish_faults(const char* path)
{
    fault.active = false; xrtMemDebugFailClear(); xrtClearError();
    for (unsigned i = 0; i < CALL_COUNT; ++i) xrtErrorFree(fault.causes[i]);
    if (xrtFileExists(path)) require(xrtFileDelete(path), "remove only this fixture's residual PID file");
    xrtClearError();
}
static void recover(void* service, const char* path)
{
    fault.active = false; xrtMemDebugFailClear(); xrtClearError();
    require(!xdaemon_holds_pidfile(service), "retired handle not retained for retry");
    require(xdaemon_release_pidfile(service), "repeat release is harmless");
    require(xdaemon_write_pidfile(service, path) && xdaemon_release_pidfile(service), "same object immediately reacquires and releases");
}

/* public release 清旧错；析构则必须保留进入时的业务首因。 */
static void cleanup_case(const char* path, unsigned mask, unsigned mode, xerrkind kind)
{
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    reset_faults(mask, kind);
    void* service = xdaemon_create("cleanup-contract");
    require(service && xdaemon_write_pidfile(service, path), "create real held PID file");
    xrtSetErrorInfo(XERR_VALUE, "test.daemon.business", 919, "pending business error");
    xerror* pending = xrtTakeError();
    if (mode >= 2) xrtSetError(pending);
    fault.active = true;
    bool destroyed = (mode & 1u) != 0;
    if (destroyed) xdaemon_destroy(service);
    else require(xdaemon_release_pidfile(service) == (mask == 0), "release return follows cleanup outcome");
    bool allocated = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); fault.active = false;
    xerror* expected = mode == 3 ? pending : mask ? fault.causes[mask & 1u ? DELETE_FILE : mask & 2u ? UNLOCK_FILE : CLOSE_FILE] : NULL;
#ifndef _WIN32
    /* POSIX 解锁失败的首因由包内 errno 路径发布，而不是我们的预置错误。 */
    if (expected == fault.causes[UNLOCK_FILE])
        require(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_IO &&
            strcmp(xrtErrorDomain(xrtGetError()), "xlang.extlib.daemon") == 0 && xrtErrorCode(xrtGetError()) == EIO,
            "first POSIX unlock error survives later close error");
    else
#endif
    require(xrtGetError() == expected, "first cleanup or pending business error retains identity");
    require(fault.calls[DELETE_FILE] == 1 && fault.calls[UNLOCK_FILE] == 1 && fault.calls[CLOSE_FILE] == 1,
        "delete/unlock/close attempted exactly once even after failure");
    /* POSIX errno -> XRT 首次发布本身可能分配；不把它误叫诊断复制分配。 */
#ifdef _WIN32
    require(!allocated, "cleanup does not allocate another diagnostic");
#else
    (void)allocated;
#endif
    if (!destroyed) { recover(service, path); xdaemon_destroy(service); }
    xrtClearError(); xrtErrorFree(pending); finish_faults(path);
    require(memory_equal(&before), "cleanup case seven memory metrics balanced");
    ++cases;
}

static void write_case(const char* path, unsigned stage, unsigned cleanup, xerrkind kind)
{
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    reset_faults((1u << stage) | cleanup, kind);
    void* service = xdaemon_create("write-contract");
    require(service != NULL, "create write test owner");
    fault.active = true;
    require(!xdaemon_write_pidfile(service, path), "write failure reported");
    xrtMemDebugFailClear(); fault.active = false;
    require(xrtGetError() == fault.causes[stage], "write first cause survives unlock/close failures");
    require(fault.calls[DELETE_FILE] == 0 && fault.calls[UNLOCK_FILE] == 1 && fault.calls[CLOSE_FILE] == 1,
        "failed write retires lock and file, no unrelated deletion");
    if (stage == COPY_PATH) require(fault.calls[FORMAT_PID] == 0, "path copy failure does not attempt formatting");
    recover(service, path); xdaemon_destroy(service); finish_faults(path);
    require(memory_equal(&before), "write failure memory balanced"); ++cases;
}

static void actual_oom(const char* path)
{
    for (unsigned budget = 0; budget < 128; ++budget) {
        xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
        reset_faults(0, XERR_IO);
        void* service = xdaemon_create("oom-owner");
        require(service != NULL && xrtMemDebugFailAfter(budget), "arm actual write allocation failure");
        fault.active = true;
        bool ok = xdaemon_write_pidfile(service, path);
        bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); fault.active = false;
        if (triggered) {
            require(!ok && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "real OOM retains memory error");
            if (fault.copyFailed) require(fault.calls[FORMAT_PID] == 0, "OOM path copy stops before formatting");
            recover(service, path); ++oomFaults;
        } else require(ok && xrtGetError() == NULL && xdaemon_release_pidfile(service), "allocation sweep reaches normal success");
        xdaemon_destroy(service); finish_faults(path);
        require(memory_equal(&before), "actual OOM owner memory balanced"); ++oomRuns;
        if (!triggered) return;
    }
    require(false, "write allocation sweep must reach success");
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.daemon", 0, "warm diagnostics"); xrtClearError();
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    char* directory = xrtDirTemp(NULL, "xlang-daemon-cleanup-", NULL);
    char* path = directory ? xrtPathJoin(directory, "owner.pid") : NULL;
    if (!path) return 2;
    static const xerrkind kinds[] = {XERR_IO, XERR_MEMORY, XERR_VALUE};
    for (unsigned k = 0; k < 3; ++k) {
        for (unsigned mask = 0; mask < 8; ++mask) for (unsigned mode = 0; mode < 4; ++mode)
            cleanup_case(path, mask, mode, kinds[k]);
        for (unsigned stage = COPY_PATH; stage < CALL_COUNT; ++stage)
            for (unsigned cleanup = 0; cleanup < 4; ++cleanup)
                write_case(path, stage, ((cleanup & 1u) << UNLOCK_FILE) | ((cleanup >> 1) << CLOSE_FILE), kinds[k]);
    }
    actual_oom(path);
    require(cases == 156 && oomFaults > 0, "full cleanup/write matrix and actual OOM executed");
    require(xrtDirRemove(directory), "remove fixture directory");
    xrtFree(path); xrtFree(directory); require(memory_equal(&before), "all owners retired");
    printf("daemon cleanup contract: %u cases, %u checks, %u budgets, %u real OOM, %u failures\n", cases, checks, oomRuns, oomFaults, failures);
    return failures ? 1 : 0;
}
