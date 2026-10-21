#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XRT_MODULE_CRYPTO_MD5
#define XRT_MODULE_CRYPTO_MD5
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_md5.h"
#include "include/md5_native.h"

bool xmd5_available(void) { return true; }
const char* xmd5_version(void) { return "1.0.0"; }
const char* xmd5_last_error(void) { return xrtErrorMessage(xrtGetError()); }

/* 文本借用 XRT 临时区，二进制 Buffer 转移给调用方；失败保留原错误。 */
static const char* md5_hex(const uint8 digest[16])
{
    size_t size;
    char* text = xrtTemp(33);
    return text != NULL && xrtHexEncode(digest, 16, text, 33, &size, 0) ? text : "";
}
static xbuffer* md5_buffer(const uint8 digest[16])
{
    xbuffer* buffer = xrtBufferCreate();
    if (buffer == NULL) return NULL;
    if (!xrtBufferAssign(buffer, (xbytesview){digest, 16})) {
        xerror* error = xrtTakeError();
        xrtBufferDestroy(buffer);
        xrtSetErrorTake(error);
        return NULL;
    }
    return buffer;
}

const char* xmd5_hash_hex(void* data, size_t size)
{
    uint8 digest[16];
    return xrtMd5Hash(data, size, digest, NULL) ? md5_hex(digest) : "";
}
const char* xmd5_hash_str_hex(const char* data, size_t size)
{ return xmd5_hash_hex((void*)data, size); }
void* xmd5_hash_bytes(void* data, size_t size)
{
    uint8 digest[16];
    return xrtMd5Hash(data, size, digest, NULL) ? md5_buffer(digest) : NULL;
}
void* xmd5_hash_str_bytes(const char* data, size_t size)
{ return xmd5_hash_bytes((void*)data, size); }

// ----------------------------- 文件与回调 -----------------------------

static bool md5_file(const char* path, uint64 maximum, xvalue* callback, uint8 digest[16])
{
    xrtresourcelimits limits;
    xrtprogresscall progress;
    xreader* reader;
    bool ok, closed;
    xerror* failure;
    xrtClearError();
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.md5", 0, "md5 file path is required");
        return false;
    }
    reader = xrtReaderOpen(path);
    if (reader == NULL) return false;
    xrtResourceLimitsInit(&limits);
    if (maximum != 0) limits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    ok = xrtMd5File(reader, &limits, digest,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    failure = !ok ? xrtTakeError() : NULL;
    closed = xrtReaderDestroy(reader);
    if (!ok) xrtSetErrorTake(failure);
    return ok && closed;
}
const char* xmd5_file_hex(const char* path, unsigned long long maximum, xvalue* callback)
{
    uint8 digest[16];
    return md5_file(path, maximum, callback, digest) ? md5_hex(digest) : "";
}
void* xmd5_file_bytes(const char* path, unsigned long long maximum, xvalue* callback)
{
    uint8 digest[16];
    return md5_file(path, maximum, callback, digest) ? md5_buffer(digest) : NULL;
}

// ----------------------------- 流式对象薄绑定 -----------------------------

void* xmd5_hasher_create(unsigned long long maximum)
{
    xrtmd5options options;
    xrtMd5OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtMd5HasherCreate(&options);
}
void xmd5_hasher_destroy(void* handle) { xrtMd5HasherDestroy(handle); }
bool xmd5_hasher_update(void* handle, void* data, size_t size)
{ return xrtMd5HasherUpdate(handle, data, size); }
bool xmd5_hasher_update_str(void* handle, const char* data, size_t size)
{ return xrtMd5HasherUpdate(handle, data, size); }
const char* xmd5_hasher_digest_hex(void* handle)
{
    uint8 digest[16];
    return xrtMd5HasherDigest(handle, digest) ? md5_hex(digest) : "";
}
void* xmd5_hasher_digest_bytes(void* handle)
{
    uint8 digest[16];
    return xrtMd5HasherDigest(handle, digest) ? md5_buffer(digest) : NULL;
}
bool xmd5_hasher_reset(void* handle) { return xrtMd5HasherReset(handle); }
unsigned long long xmd5_hasher_input_bytes(void* handle)
{ return (unsigned long long)xrtMd5HasherInputBytes(handle); }
