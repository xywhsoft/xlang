#define XRT_MODULE_ALL
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "../include/xrt_blake3.h"
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
} close_probe;

static close_probe Probe;

static bool read_probe(ptr context, ptr data, size_t request, size_t* count)
{
    close_probe* probe = context;
    size_t remaining = strlen(probe->Text) - probe->Offset;
    if (probe->FailRead) {
        xrtSetErrorInfo(XERR_IO, "test.blake3", 0, "read marker");
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
    xrtSetErrorInfo(XERR_IO, "test.blake3", 0, "close marker");
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
#include "../blake3_native.c"
#undef xrtReaderOpen

int main(void)
{
    char expected[65];
    strcpy(expected, xblake3_hash_str_hex("abc", 3, 32));

    Probe = (close_probe){"abc", 0, 0, false, true};
    assert(!xblake3_file_hex("injected", 0, 32, NULL)[0]);
    assert(Probe.Closed == 1 && Probe.Offset == 3);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    assert(strcmp(xrtErrorMessage(xrtGetError()), "close marker") == 0);

    Probe = (close_probe){"abc", 0, 0, true, true};
    assert(!xblake3_file_hex("injected", 0, 32, NULL)[0]);
    assert(Probe.Closed == 1 && xrtErrorKind(xrtGetError()) == XERR_IO);
    assert(strcmp(xrtErrorMessage(xrtGetError()), "read marker") == 0);

    Probe = (close_probe){"abc", 0, 0, false, false};
    assert(strcmp(xblake3_file_hex("injected", 0, 32, NULL), expected) == 0);
    assert(Probe.Closed == 1 && xrtGetError() == NULL);
    assert(xrtTempClear());
    puts("blake3 file close: PASS");
    return 0;
}
