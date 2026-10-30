#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE_TEXT
#include "src/common/xl6_xrt.h"
#include "xl6_text_value.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

static bool append_argument(xvalue* output, xstrview text)
{
    if (!xrtUtf8Valid(text, NULL)) {
        xrtSetErrorKind(XERR_VALUE);
        return false;
    }
    xvalue* argument = xrtValueString(text);
    return argument && xrtValueArrayAppendNew(output, argument);
}

/* Host process argv, including argv[0]. Script/main arguments are a separate
 * language call contract. Always return an independent owned array. */
xvalue* xl6_xrt_os_args(void)
{
    xvalue* output = xrtValueArray();
    if (!output) return NULL;
#if defined(_WIN32)
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) {
        DWORD code = GetLastError();
        xrtValueRelease(output);
        xrtSetErrorTake(xrtErrorCreate(code == ERROR_NOT_ENOUGH_MEMORY ? XERR_MEMORY : XERR_IO,
            "win32.commandLine", code, "cannot obtain Unicode process arguments"));
        return NULL;
    }
    bool ok = true;
    for (int i = 0; i < count && ok; ++i) {
        size_t length = 0;
        char* text = xrtUtf16ViewTo8(xrtUtf16View((const uint16*)arguments[i],
            xrtUtf16Len((const uint16*)arguments[i])), XUTF_STRICT, &length);
        ok = text && append_argument(output, (xstrview){text, length});
        xrtFree(text);
    }
    xerror* error = ok ? NULL : xrtTakeError();
    if (LocalFree(arguments) && ok) {
        ok = false;
        error = xrtErrorCreate(XERR_IO, "win32.commandLine", GetLastError(),
            "cannot release Unicode process arguments");
    }
    if (!ok) {
        xrtValueRelease(output);
        xrtSetErrorTake(error);
        return NULL;
    }
#elif defined(__linux__)
    /* Linux exposes the actual process vector as NUL-separated bytes. Do not
     * use shell parsing, strlen on the complete vector, or stat's zero size. */
    const size_t limit = 16u * 1024u * 1024u;
    char* data = NULL;
    size_t size = 0, capacity = 0;
    FILE* input = fopen("/proc/self/cmdline", "rb");
    bool ok = input != NULL;
    if (!ok) xrtSetErrorTake(xrtErrorCreate(XERR_IO, "posix.errno", errno,
        "cannot open process arguments"));
    while (ok) {
        if (size == capacity) {
            if (capacity == limit) {
                if (fgetc(input) != EOF) {
                    xrtSetErrorKind(XERR_RANGE);
                    ok = false;
                }
                break;
            }
            size_t next = capacity ? capacity * 2u : 4096u;
            char* grown = xrtRealloc(data, next);
            if (!grown) { ok = false; break; }
            data = grown;
            capacity = next;
        }
        size_t count = fread(data + size, 1, capacity - size, input);
        size += count;
        if (!count) break;
    }
    if (input && ferror(input)) {
        if (ok) xrtSetErrorTake(xrtErrorCreate(XERR_IO, "posix.errno", errno,
            "cannot read process arguments"));
        ok = false;
    }
    xerror* error = ok ? NULL : xrtTakeError();
    if (input && fclose(input) && ok) {
        ok = false;
        error = xrtErrorCreate(XERR_IO, "posix.errno", errno, "cannot close process arguments");
    }
    if (ok && size && data[size - 1] != '\0') {
        ok = false;
        error = xrtErrorCreate(XERR_VALUE, "os.arguments", 0, "incomplete process argument vector");
    }
    for (size_t offset = 0; ok && offset < size;) {
        const char* end = memchr(data + offset, 0, size - offset);
        size_t length = (size_t)(end - (data + offset));
        ok = append_argument(output, (xstrview){data + offset, length});
        if (!ok) error = xrtTakeError();
        offset += length + 1u;
    }
    xrtFree(data);
    if (!ok) {
        xrtValueRelease(output);
        xrtSetErrorTake(error);
        return NULL;
    }
#else
    xrtValueRelease(output);
    xrtSetErrorKind(XERR_UNSUPPORTED);
    return NULL;
#endif
    return output;
}
