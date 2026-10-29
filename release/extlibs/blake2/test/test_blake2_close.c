#define XRT_MODULE_ALL
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

static bool read_probe(ptr context, ptr data, size_t request, size_t* count)
{
    close_probe* probe = context;
    size_t remaining = strlen(probe->Text) - probe->Offset;
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

int main(void)
{
    check_file(true);
    check_file(false);
    check_memory_read(true);
    check_memory_read(false);
    puts("blake2 file close: PASS");
    return 0;
}
