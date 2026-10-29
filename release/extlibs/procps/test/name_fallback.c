#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/procps_native.h"
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

static xerrkind read_error;
static const char* input;
static size_t input_size;
static bytes provided;
static unsigned reads, cases, failures;

/* 强制走 comm 回退分支，避免依赖测试机上的真实进程状态。 */
ssize_t __wrap_readlink(const char* path, char* buffer, size_t capacity)
{
    (void)path;
    (void)buffer;
    (void)capacity;
    errno = ENOENT;
    return -1;
}

ssize_t __wrap___readlink_chk(const char* path, char* buffer,
    size_t capacity, size_t buffer_size)
{
    (void)buffer_size;
    return __wrap_readlink(path, buffer, capacity);
}

bytes __wrap_xrtFileReadAllLimit(cstr path, size_t limit, size_t* size)
{
    (void)path;
    ++reads;
    if (read_error != XERR_NONE) {
        xrtSetErrorInfo(read_error, "procps.test", 0, "injected comm read error");
        return NULL;
    }
    if (input_size > limit) return NULL;
    provided = xrtMalloc(input_size + 1u);
    if (provided == NULL) return NULL;
    memcpy(provided, input, input_size);
    provided[input_size] = 0;
    if (size != NULL) *size = input_size;
    return provided;
}

static void check(bool ok, const char* label)
{
    ++cases;
    if (!ok) { ++failures; fprintf(stderr, "procps comm case %u: %s\n", cases, label); }
}

int main(void)
{
    static const struct { const char* raw; size_t size; const char* expected; } texts[] = {
        {"worker\n", 7, "worker"}, {"worker\r\n", 8, "worker\r"},
        {"a\nb\n", 4, "a\nb"}, {"a\rb\n", 4, "a\rb"}, {"a\n\n", 3, "a\n"},
        {"\n", 1, ""}, {"\n\n", 2, "\n"}, {"name", 4, "name"}, {"", 0, ""},
        {" \tworker \n", 10, " \tworker "},
        {"\xe4\xbd\xa0\n\xe5\xa5\xbd\n", 8, "\xe4\xbd\xa0\n\xe5\xa5\xbd"},
        {"a\0b\n", 4, NULL}, {"a\n\xff\n", 4, NULL},
        {"\xff\n", 2, NULL}, {"\xc3\n", 2, NULL}, {"valid\n\0tail", 11, NULL}
    };
    static const xerrkind errors[] = {XERR_NOT_FOUND, XERR_PERMISSION, XERR_IO, XERR_MEMORY};
    xmemdebugsnapshot before, after;
    xrtSetErrorInfo(XERR_IO, "procps.test.old", 0, "warm diagnostic slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i) {
        input = texts[i].raw; input_size = texts[i].size; reads = 0; provided = NULL;
        xrtSetErrorInfo(XERR_IO, "procps.test.old", 0, "old diagnostic");
        char* result = xprocps_name(1);
        check(reads == 1 && (texts[i].expected ? result == (char*)provided && result &&
            strcmp(result, texts[i].expected) == 0 && !xrtGetError() :
            !result && xrtErrorKind(xrtGetError()) == XERR_VALUE), "raw comm bytes");
        xrtFree(result);
        xrtClearError();
    }
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        read_error = errors[i];
        char* result = xprocps_name(1);
        check(!result && (read_error == XERR_MEMORY ?
            xrtErrorKind(xrtGetError()) == XERR_MEMORY : !xrtGetError()), "unavailable versus memory");
        xrtFree(result);
        xrtClearError();
    }
    read_error = XERR_NONE; input = "a\nb\n"; input_size = 4;
    for (unsigned budget = 0; budget < 2; ++budget) {
        if (!xrtMemDebugFailAfter(budget)) return 2;
        char* result = xprocps_name(1);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        check(budget == 0 ? !result && injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            result && !injected && strcmp(result, "a\nb") == 0 && !xrtGetError(), "owned fallback budget");
        xrtFree(result);
        xrtClearError();
    }
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount, "allocation counters");
    printf("procps comm fallback: %u checks, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
