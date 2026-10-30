#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "../include/xrt_blake2.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* Text;
    size_t Offset;
    int Closed;
    bool FailRead;
    bool FailClose;
    bool MemoryRead;
} close_probe;

static close_probe Probe;
static xerror* ReadCause;
static xerror* CloseCause;
static bool FailDiagnostic;

static bool read_probe(ptr context, ptr data, size_t request, size_t* count)
{
    close_probe* probe = context;
    size_t remaining = strlen(probe->Text) - probe->Offset;
    if (ReadCause != NULL) {
        xrtSetError(ReadCause);
        /* 首错已经完整发布，只针对其后的诊断/清理分配注入故障。 */
        if (FailDiagnostic) assert(xrtMemDebugFailAfter(0));
        return false;
    }
    if (probe->FailRead) {
        xrtSetErrorInfo(probe->MemoryRead ? XERR_MEMORY : XERR_IO,
            "test.blake2", 0, "read marker");
        return false;
    }
    *count = request < remaining ? request : remaining;
    if (*count != 0) memcpy(data, probe->Text + probe->Offset, *count);
    probe->Offset += *count;
    return true;
}

static bool close_probe_reader(ptr context)
{
    close_probe* probe = context;
    probe->Closed++;
    if (CloseCause != NULL) {
        xrtSetError(CloseCause);
        return false;
    }
    if (!probe->FailClose) return true;
    xrtSetErrorInfo(XERR_IO, "test.blake2", 0, "close marker");
    return false;
}

static xreader* open_probe(const char* path)
{
    xreaderops ops = {0};
    (void)path;
    ops.Read = read_probe;
    ops.Close = close_probe_reader;
    return xrtReaderCreate(&ops, &Probe);
}

/* 测试公开薄绑定，但只替换测试编译单元里的文件打开入口。 */
#define xrtReaderOpen open_probe
#include "../blake2_native.c"
#undef xrtReaderOpen

static void check_file(bool large)
{
    char expected[129];
    const char* result;
    size_t size = large ? 64 : 32;
    strcpy(expected, large ? xblake2b_hash_str_hex("abc", 3, size, NULL, 0) :
        xblake2s_hash_str_hex("abc", 3, size, NULL, 0));

    Probe = (close_probe){"abc", 0, 0, false, true, false};
    result = large ? xblake2b_file_hex("injected", 0, size, NULL) :
        xblake2s_file_hex("injected", 0, size, NULL);
    assert(!result[0] && Probe.Closed == 1 && Probe.Offset == 3);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    assert(strcmp(xrtErrorMessage(xrtGetError()), "close marker") == 0);

    Probe = (close_probe){"abc", 0, 0, true, true, false};
    result = large ? xblake2b_file_hex("injected", 0, size, NULL) :
        xblake2s_file_hex("injected", 0, size, NULL);
    assert(!result[0] && Probe.Closed == 1);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    assert(strcmp(xrtErrorMessage(xrtGetError()), "close marker") != 0);

    Probe = (close_probe){"abc", 0, 0, false, false, false};
    result = large ? xblake2b_file_hex("injected", 0, size, NULL) :
        xblake2s_file_hex("injected", 0, size, NULL);
    assert(strcmp(result, expected) == 0 && Probe.Closed == 1);
    assert(xrtGetError() == NULL);
    assert(xrtTempClear());
}

static void check_memory_read(bool large)
{
    uint8 digest[64];
    xrtblake2error error;
    xreader* reader;
    const char* result;
    size_t size = large ? 64 : 32;
    Probe = (close_probe){"abc", 0, 0, true, false, true};
    reader = open_probe("injected");
    assert(reader != NULL);
    xrtClearError();
    assert(!(large ? xrtBlake2bFile(reader, NULL, size, digest, NULL, NULL, &error) :
        xrtBlake2sFile(reader, NULL, size, digest, NULL, NULL, &error)));
    assert(error.iCode == XRT_BLAKE2_ERROR_MEMORY);
    assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    assert(xrtReaderDestroy(reader) && Probe.Closed == 1);

    Probe = (close_probe){"abc", 0, 0, true, false, true};
    result = large ? xblake2b_file_hex("injected", 0, size, NULL) :
        xblake2s_file_hex("injected", 0, size, NULL);
    assert(!result[0] && Probe.Closed == 1);
    assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtClearError();
}

/* 关闭失败和诊断OOM都不能替换读取首错，连同底层cause完整保留。 */
static int check_first_error(bool large, bool memory, int close_kind, bool oom)
{
    xmemdebugsnapshot before, after;
    xerrordesc desc = {0};
    xerror* underlying;
    const xerror* actual;
    const char* result;
    bool good, injected;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    underlying = xrtErrorCreate(XERR_IO, "test.blake2.root", 801, "root marker");
    desc.Kind = memory ? XERR_MEMORY : XERR_IO;
    desc.Domain = "test.blake2.reader";
    desc.Code = 731;
    desc.SystemCode = 812;
    desc.Operation = "read";
    desc.Message = "first read marker";
    desc.Data = "original detail";
    desc.Cause = underlying;
    ReadCause = xrtErrorBuild(&desc);
    CloseCause = close_kind == 0 ? NULL : xrtErrorCreate(
        close_kind == 1 ? XERR_IO : XERR_MEMORY, "test.blake2.close", 913, "secondary close marker");
    assert(underlying && ReadCause && (close_kind == 0 || CloseCause));
    Probe = (close_probe){"abc", 0, 0, false, false, false};
    FailDiagnostic = oom;
    result = large ? xblake2b_file_hex("injected", 0, 16, NULL) :
        xblake2s_file_hex("injected", 0, 16, NULL);
    injected = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    FailDiagnostic = false;
    actual = xrtGetError();
    good = result && !*result && Probe.Closed == 1 && !injected && actual == ReadCause &&
        xrtErrorKind(actual) == desc.Kind && xrtErrorCode(actual) == desc.Code &&
        xrtErrorSystemCode(actual) == desc.SystemCode &&
        !strcmp(xrtErrorDomain(actual), desc.Domain) &&
        !strcmp(xrtErrorOperation(actual), desc.Operation) &&
        !strcmp(xrtErrorMessage(actual), desc.Message) &&
        !strcmp(xrtErrorData(actual), desc.Data) && xrtErrorCause(actual) == underlying;
    if (!good) fprintf(stderr, "blake2 first error: b=%d memory=%d close=%d oom=%d kind=%d code=%d secondary-allocation=%d\n",
        large, memory, close_kind, oom, xrtErrorKind(actual), xrtErrorCode(actual), injected);
    xrtClearError();
    xrtErrorFree(ReadCause);
    xrtErrorFree(CloseCause);
    xrtErrorFree(underlying);
    ReadCause = CloseCause = NULL;
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    /* 同一入口故障后立即恢复，避免旧错误污染后续成功。 */
    check_file(large);
    return !good;
}

/* 算法自身的限额错误没有Reader首错，不得误恢复随后关闭产生的错误。 */
static void check_limit_error(bool large, int close_kind)
{
    Probe = (close_probe){"abc", 0, 0, false, false, false};
    CloseCause = close_kind == 0 ? NULL : xrtErrorCreate(
        close_kind == 1 ? XERR_IO : XERR_MEMORY, "test.blake2.close", 913, "secondary close marker");
    assert(close_kind == 0 || CloseCause);
    const char* text = large ? xblake2b_file_hex("injected", 1, 16, NULL) :
        xblake2s_file_hex("injected", 1, 16, NULL);
    assert(text && !*text && Probe.Closed == 1 && Probe.Offset == 3);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!strcmp(xrtErrorDomain(xrtGetError()), "xlang.extlib.blake2"));
    assert(strstr(xrtErrorMessage(xrtGetError()), "maxInput") != NULL);
    xrtClearError();
    xrtErrorFree(CloseCause);
    CloseCause = NULL;
}

int main(void)
{
    int failures = 0;
    assert(xrtMemDebugEnable(true));
    check_file(true);
    check_file(false);
    check_memory_read(true);
    check_memory_read(false);
    for (int large = 0; large < 2; ++large)
        for (int memory = 0; memory < 2; ++memory)
            for (int close_kind = 0; close_kind < 3; ++close_kind)
                for (int oom = 0; oom < 2; ++oom)
                    failures += check_first_error(large != 0, memory != 0, close_kind, oom != 0);
    for (int large = 0; large < 2; ++large)
        for (int close_kind = 0; close_kind < 3; ++close_kind)
            check_limit_error(large != 0, close_kind);
    puts("blake2 file close: PASS");
    printf("blake2 first error: 24 read/close/OOM cases + 6 limit controls, failures=%d\n", failures);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    return failures ? 1 : 0;
}
