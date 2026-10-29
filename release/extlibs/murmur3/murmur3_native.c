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
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_murmur3.h"
#include "include/murmur3_native.h"

bool xmurmur3_available(void) { return true; }
const char* xmurmur3_version(void) { return "3.0.0"; }
const char* xmurmur3_last_error(void) { return xrtErrorMessage(xrtGetError()); }

// ----------------------------- 摘要的文本与 Buffer 视图 -----------------------------

static const char* murmur3_hex(const uint8* digest, size_t size)
{
    size_t written;
    char* text = xrtTemp(size * 2 + 1);
    return text != NULL && xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0) ? text : "";
}
static xbuffer* murmur3_buffer(const uint8* digest, size_t size)
{
    xbuffer* buffer = xrtBufferCreate();
    if (buffer == NULL) return NULL;
    if (!xrtBufferAssign(buffer, (xbytesview){digest, size})) {
        xerror* error = xrtTakeError();
        xrtBufferDestroy(buffer);
        xrtSetErrorTake(error);
        return NULL;
    }
    return buffer;
}
static bool murmur3_bytes32(void* data, size_t size, unsigned int seed, uint8 digest[4])
{
    uint32 value = xrtMurmur3Hash32(data, size, seed, NULL);
    unsigned i;
    if (xrtGetError() != NULL) return false;
    /* 与 128 位输出统一为小端字节，而不是主机整数内存布局。 */
    for (i = 0; i < 4; ++i) digest[i] = (uint8)(value >> (i * 8));
    return true;
}

long long xmurmur3_hash32(void* data, size_t size, unsigned int seed)
{ return xrtMurmur3Hash32(data, size, seed, NULL); }
long long xmurmur3_hash32_str(const char* data, size_t size, unsigned int seed)
{ return xmurmur3_hash32((void*)data, size, seed); }
const char* xmurmur3_hash32_hex(void* data, size_t size, unsigned int seed)
{
    uint8 digest[4];
    return murmur3_bytes32(data, size, seed, digest) ? murmur3_hex(digest, 4) : "";
}
const char* xmurmur3_hash32_str_hex(const char* data, size_t size, unsigned int seed)
{ return xmurmur3_hash32_hex((void*)data, size, seed); }
void* xmurmur3_hash32_bytes(void* data, size_t size, unsigned int seed)
{
    uint8 digest[4];
    return murmur3_bytes32(data, size, seed, digest) ? murmur3_buffer(digest, 4) : NULL;
}
void* xmurmur3_hash32_str_bytes(const char* data, size_t size, unsigned int seed)
{ return xmurmur3_hash32_bytes((void*)data, size, seed); }
const char* xmurmur3_hash128_hex(void* data, size_t size, unsigned int seed)
{
    uint8 digest[16];
    return xrtMurmur3Hash128(data, size, seed, digest, NULL) ? murmur3_hex(digest, 16) : "";
}
const char* xmurmur3_hash128_str_hex(const char* data, size_t size, unsigned int seed)
{ return xmurmur3_hash128_hex((void*)data, size, seed); }
void* xmurmur3_hash128_bytes(void* data, size_t size, unsigned int seed)
{
    uint8 digest[16];
    return xrtMurmur3Hash128(data, size, seed, digest, NULL) ? murmur3_buffer(digest, 16) : NULL;
}
void* xmurmur3_hash128_str_bytes(const char* data, size_t size, unsigned int seed)
{ return xmurmur3_hash128_bytes((void*)data, size, seed); }

// ----------------------------- 文件与同步回调 -----------------------------

static bool murmur3_path(const char* path, uint64 maximum, uint32 seed, xvalue* callback, bool wide, void* digest)
{
    xrtresourcelimits limits;
    xrtprogresscall progress;
    xreader* reader;
    bool ok, closed;
    xerror* failure;
    xrtClearError();
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.murmur3", 0, "murmur3 file path is required");
        return false;
    }
    reader = xrtReaderOpen(path);
    if (reader == NULL) return false;
    xrtResourceLimitsInit(&limits);
    if (maximum != 0) limits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    if (wide) {
        ok = xrtMurmur3File128(reader, &limits, seed, digest,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    } else {
        *(uint32*)digest = xrtMurmur3File32(reader, &limits, seed,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
        ok = xrtGetError() == NULL;
    }
    failure = !ok ? xrtTakeError() : NULL;
    closed = xrtReaderDestroy(reader);
    if (!ok) xrtSetErrorTake(failure);
    return ok && closed;
}
long long xmurmur3_file32(const char* path, unsigned long long maximum, unsigned int seed, xvalue* callback)
{
    uint32 digest;
    return murmur3_path(path, maximum, seed, callback, false, &digest) ? digest : 0;
}
const char* xmurmur3_file128_hex(const char* path, unsigned long long maximum, unsigned int seed, xvalue* callback)
{
    uint8 digest[16];
    return murmur3_path(path, maximum, seed, callback, true, digest) ? murmur3_hex(digest, 16) : "";
}

// ----------------------------- 流式对象薄绑定 -----------------------------

void* xmurmur3_hasher_create(unsigned int seed, unsigned long long maximum)
{
    xrtmurmur3options options;
    xrtMurmur3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtMurmur3HasherCreate(&options, seed);
}
void xmurmur3_hasher_destroy(void* handle) { xrtMurmur3HasherDestroy(handle); }
bool xmurmur3_hasher_update(void* handle, void* data, size_t size)
{ return xrtMurmur3HasherUpdate(handle, data, size); }
bool xmurmur3_hasher_update_str(void* handle, const char* data, size_t size)
{ return xrtMurmur3HasherUpdate(handle, data, size); }
const char* xmurmur3_hasher_digest_hex(void* handle)
{
    uint8 digest[16];
    return xrtMurmur3HasherDigest(handle, digest) ? murmur3_hex(digest, 16) : "";
}
void* xmurmur3_hasher_digest_bytes(void* handle)
{
    uint8 digest[16];
    return xrtMurmur3HasherDigest(handle, digest) ? murmur3_buffer(digest, 16) : NULL;
}
bool xmurmur3_hasher_reset(void* handle) { return xrtMurmur3HasherReset(handle); }
unsigned long long xmurmur3_hasher_input_bytes(void* handle)
{ return xrtMurmur3HasherInputBytes(handle); }
void* xmurmur3_hasher_adopt(void* handle)
{
    xrtClearError();
    if (handle == NULL) xrtSetErrorInfo(XERR_STATE, "xlang.extlib.murmur3", 0, "invalid murmur3 handle");
    return handle;
}
void* xmurmur3_hasher_clone(void* handle) { return xrtMurmur3HasherClone(handle); }
