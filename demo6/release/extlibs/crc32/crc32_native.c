#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_crc32.h"
#include "include/crc32_native.h"
#include <stdio.h>

bool xcrc32_available(void) { return true; }
const char* xcrc32_version(void) { return "1.0.0"; }
const char* xcrc32_last_error(void) { return xrtErrorMessage(xrtGetError()); }

/* 借用 XRT 当前临时区，后续摘要和其他线程不会覆盖返回文本。 */
static const char* crc32_hex(uint32 value)
{
    char* text = xrtTemp(9);
    if (text == NULL) return "";
    snprintf(text, 9, "%08x", (unsigned int)value);
    return text;
}

long long xcrc32_hash(void* data, size_t size)
{
    uint32 crc = 0;
    xrtCrc32Hash(data, size, &crc, NULL);
    return (long long)crc;
}
long long xcrc32_hash_str(const char* data, size_t size)
{ return xcrc32_hash((void*)data, size); }
const char* xcrc32_hash_hex(void* data, size_t size)
{
    uint32 crc;
    return xrtCrc32Hash(data, size, &crc, NULL) ? crc32_hex(crc) : "";
}
const char* xcrc32_hash_str_hex(const char* data, size_t size)
{ return xcrc32_hash_hex((void*)data, size); }

// ----------------------------- 文件与回调 -----------------------------

static bool crc32_file(const char* path, uint64 maximum, xvalue* callback, uint32* crc)
{
    xrtresourcelimits limits;
    xrtprogresscall progress;
    xreader* reader;
    bool ok, closed;
    xerror* failure;
    xrtClearError();
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.crc32", 0, "crc32 file path is required");
        return false;
    }
    reader = xrtReaderOpen(path);
    if (reader == NULL) return false;
    xrtResourceLimitsInit(&limits);
    if (maximum != 0) limits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    ok = xrtCrc32File(reader, &limits, crc,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    failure = !ok ? xrtTakeError() : NULL;
    closed = xrtReaderDestroy(reader);
    if (!ok) xrtSetErrorTake(failure);
    return ok && closed;
}
long long xcrc32_file(const char* path, unsigned long long maximum, xvalue* callback)
{
    uint32 crc = 0;
    return crc32_file(path, maximum, callback, &crc) ? (long long)crc : 0;
}
const char* xcrc32_file_hex(const char* path, unsigned long long maximum, xvalue* callback)
{
    uint32 crc;
    return crc32_file(path, maximum, callback, &crc) ? crc32_hex(crc) : "";
}

// ----------------------------- 流式对象薄绑定 -----------------------------

void* xcrc32_hasher_create(unsigned long long maximum)
{
    xrtcrc32options options;
    xrtCrc32OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtCrc32HasherCreate(&options);
}
void xcrc32_hasher_destroy(void* handle) { xrtCrc32HasherDestroy(handle); }
bool xcrc32_hasher_update(void* handle, void* data, size_t size)
{ return xrtCrc32HasherUpdate(handle, data, size); }
bool xcrc32_hasher_update_str(void* handle, const char* data, size_t size)
{ return xrtCrc32HasherUpdate(handle, data, size); }
long long xcrc32_hasher_digest(void* handle)
{
    uint32 crc = 0;
    xrtCrc32HasherDigest(handle, &crc);
    return (long long)crc;
}
const char* xcrc32_hasher_digest_hex(void* handle)
{
    uint32 crc;
    return xrtCrc32HasherDigest(handle, &crc) ? crc32_hex(crc) : "";
}
bool xcrc32_hasher_reset(void* handle) { return xrtCrc32HasherReset(handle); }
unsigned long long xcrc32_hasher_input_bytes(void* handle)
{ return (unsigned long long)xrtCrc32HasherInputBytes(handle); }
