#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_blake3.h"
#include "include/blake3_native.h"
#include "third_party/blake3.h"

#if defined(_WIN32)
#define XBLAKE3_EXPORT __declspec(dllexport)
#else
#define XBLAKE3_EXPORT
#endif

static bool blake3_key(const void* key, size_t size)
{
    if (key != NULL && size == XRT_BLAKE3_KEY_LEN) return true;
    xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake3", 0, "blake3 key must be exactly 32 bytes");
    return false;
}

static bool blake3_size(size_t size)
{
    xrtresourcelimits limits;
    if (size == 0 || size > (SIZE_MAX - 1) / 3) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake3", 0, "invalid blake3 output size");
        return false;
    }
    /* 所有分配型接口在分配前使用同一个 XRT 默认输出预算。 */
    xrtResourceLimitsInit(&limits);
    if (limits.iMaxOutputBytes != 0 && size > limits.iMaxOutputBytes) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.blake3", 0, "blake3 output exceeds maxOutput limit");
        return false;
    }
    return true;
}

/* 摘要和 hex 共用一次临时区分配；没有环形槽或平台相关的 TLS 静态结果。 */
static char* blake3_hex_space(size_t size, uint8** digest)
{
    if (!blake3_size(size)) return NULL;
    *digest = xrtTemp(size * 3 + 1);
    return *digest != NULL ? (char*)*digest + size : NULL;
}

static const char* blake3_hex_finish(uint8* digest, size_t size, char* text)
{
    size_t written;
    bool ok = xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0);
    xrtSecureZero(digest, size);
    return ok ? text : "";
}

/* 语言侧接管此分配；C 侧原有借用型接口继续使用临时区。 */
static char* blake3_hex_owned(uint8* digest, size_t size)
{
    size_t written;
    char* text = xrtMalloc(size * 2 + 1);
    bool ok = text != NULL && xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0);
    xrtSecureZero(digest, size);
    if (!ok) { xrtFree(text); return NULL; }
    return text;
}

static xbuffer* blake3_buffer(size_t size, uint8** digest)
{
    xbuffer* result;
    if (!blake3_size(size)) return NULL;
    result = xrtBufferCreate();
    if (result == NULL) return NULL;
    *digest = xrtBufferAdd(result, size);
    if (*digest == NULL) { xrtBufferDestroy(result); return NULL; }
    return result;
}

XBLAKE3_EXPORT bool xblake3_available(void) { return true; }
XBLAKE3_EXPORT const char* xblake3_version(void) { return blake3_version(); }
XBLAKE3_EXPORT const char* xblake3_last_error(void) { return xrtErrorMessage(xrtGetError()); }

static const char* blake3_hash_hex(int mode, const void* data, size_t size,
    const void* key, size_t keylen, const char* context, size_t context_len,
    size_t outlen)
{
    uint8* digest;
    char* text;
    bool ok;
    xrtClearError();
    if (mode == 1 && !blake3_key(key, keylen)) return "";
    text = blake3_hex_space(outlen, &digest);
    if (text == NULL) return "";
    if (mode == 1) ok = xrtBlake3HashKeyed(data, size, key, digest, outlen, NULL);
    else if (mode == 2) ok = xrtBlake3HashKdf(data, size, context, context_len, digest, outlen, NULL);
    else ok = xrtBlake3Hash(data, size, digest, outlen, NULL);
    return ok ? blake3_hex_finish(digest, outlen, text) : "";
}

static char* blake3_hash_hex_owned(int mode, const void* data, size_t size,
    const void* key, size_t keylen, const char* context, size_t context_len,
    size_t outlen)
{
    uint8* digest;
    bool ok;
    xrtClearError();
    if (mode == 1 && !blake3_key(key, keylen)) return NULL;
    if (!blake3_size(outlen)) return NULL;
    digest = xrtTemp(outlen);
    if (digest == NULL) return NULL;
    if (mode == 1) ok = xrtBlake3HashKeyed(data, size, key, digest, outlen, NULL);
    else if (mode == 2) ok = xrtBlake3HashKdf(data, size, context, context_len, digest, outlen, NULL);
    else ok = xrtBlake3Hash(data, size, digest, outlen, NULL);
    if (!ok) { xrtSecureZero(digest, outlen); return NULL; }
    return blake3_hex_owned(digest, outlen);
}

XBLAKE3_EXPORT const char* xblake3_hash_hex(void* data, size_t size, size_t outlen)
{ return blake3_hash_hex(0, data, size, NULL, 0, NULL, 0, outlen); }
XBLAKE3_EXPORT const char* xblake3_hash_str_hex(const char* data, size_t size, size_t outlen)
{ return xblake3_hash_hex((void*)data, size, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_hex_owned(void* data, size_t size, size_t outlen)
{ return blake3_hash_hex_owned(0, data, size, NULL, 0, NULL, 0, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_str_hex_owned(const char* data, size_t size, size_t outlen)
{ return xblake3_hash_hex_owned((void*)data, size, outlen); }
XBLAKE3_EXPORT const char* xblake3_hash_mac_hex(void* data, size_t size,
    void* key, size_t keylen, size_t outlen)
{ return blake3_hash_hex(1, data, size, key, keylen, NULL, 0, outlen); }
XBLAKE3_EXPORT const char* xblake3_hash_mac_str_hex(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen)
{ return xblake3_hash_mac_hex((void*)data, size, (void*)key, keylen, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_mac_hex_owned(void* data, size_t size,
    void* key, size_t keylen, size_t outlen)
{ return blake3_hash_hex_owned(1, data, size, key, keylen, NULL, 0, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_mac_str_hex_owned(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen)
{ return xblake3_hash_mac_hex_owned((void*)data, size, (void*)key, keylen, outlen); }
XBLAKE3_EXPORT const char* xblake3_hash_kdf_hex(void* data, size_t size,
    const char* context, size_t context_len, size_t outlen)
{ return blake3_hash_hex(2, data, size, NULL, 0, context, context_len, outlen); }
XBLAKE3_EXPORT const char* xblake3_hash_kdf_str_hex(const char* data, size_t size,
    const char* context, size_t context_len, size_t outlen)
{ return xblake3_hash_kdf_hex((void*)data, size, context, context_len, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_kdf_hex_owned(void* data, size_t size,
    const char* context, size_t context_len, size_t outlen)
{ return blake3_hash_hex_owned(2, data, size, NULL, 0, context, context_len, outlen); }
XBLAKE3_EXPORT char* xblake3_hash_kdf_str_hex_owned(const char* data, size_t size,
    const char* context, size_t context_len, size_t outlen)
{ return xblake3_hash_kdf_hex_owned((void*)data, size, context, context_len, outlen); }

XBLAKE3_EXPORT void* xblake3_hash_bytes(void* data, size_t size, size_t outlen)
{
    uint8* digest;
    xbuffer* buffer;
    xrtClearError();
    buffer = blake3_buffer(outlen, &digest);
    if (buffer == NULL) return NULL;
    if (!xrtBlake3Hash(data, size, digest, outlen, NULL)) {
        xrtBufferDestroy(buffer);
        return NULL;
    }
    return buffer;
}
XBLAKE3_EXPORT void* xblake3_hash_str_bytes(const char* data, size_t size, size_t outlen)
{ return xblake3_hash_bytes((void*)data, size, outlen); }

/* 文件回调可重入另一文件哈希；所有状态局部化，最终结果不跨回调借用临时区。 */
static const char* blake3_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback, bool owned)
{
    xrtblake3options options;
    xrtprogresscall progress;
    xreader* reader;
    xbuffer* buffer;
    uint8* digest;
    char* text;
    size_t written;
    bool ok;
    const char* result = "";
    xrtClearError();
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake3", 0, "blake3 file path is required");
        return "";
    }
    buffer = blake3_buffer(outlen, &digest);
    if (buffer == NULL) return "";
    reader = xrtReaderOpen(path);
    if (reader == NULL) { xrtBufferDestroy(buffer); return ""; }
    xrtBlake3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    ok = xrtBlake3File(reader, &options.tLimits, digest, outlen,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    {
        xerror* first = ok ? NULL : xrtTakeError();
        bool closed = xrtReaderDestroy(reader);
        if (first != NULL) xrtSetErrorTake(first);
        if (!closed) ok = false;
    }
    if (ok) {
        if (owned) result = blake3_hex_owned(digest, outlen);
        else {
            text = xrtTemp(outlen * 2 + 1);
            if (text != NULL && xrtHexEncode(digest, outlen, text, outlen * 2 + 1, &written, 0)) result = text;
        }
    }
    xrtSecureZero(digest, outlen);
    xrtBufferDestroy(buffer);
    return result;
}

XBLAKE3_EXPORT const char* xblake3_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback)
{ return blake3_file_hex(path, maximum, outlen, callback, false); }

XBLAKE3_EXPORT char* xblake3_file_hex_owned(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback)
{
    const char* text = blake3_file_hex(path, maximum, outlen, callback, true);
    return text != NULL && text[0] != '\0' ? (char*)text : NULL;
}

XBLAKE3_EXPORT void* xblake3_hasher_create(unsigned long long maximum)
{
    xrtblake3options options;
    xrtBlake3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtBlake3HasherCreate(&options);
}
XBLAKE3_EXPORT void* xblake3_hasher_create_keyed(void* key, size_t keylen, unsigned long long maximum)
{
    xrtblake3options options;
    xrtClearError();
    if (!blake3_key(key, keylen)) return NULL;
    xrtBlake3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtBlake3HasherCreateKeyed(&options, key);
}
/* 语言入口以 bytes 自带长度校验密钥，不接受独立的裸指针长度。 */
XBLAKE3_EXPORT void* xblake3_hasher_create_keyed_bytes(xvalue* key,
    unsigned long long maximum)
{
    xbytesview view = {0};
    xrtClearError();
    if (key == NULL || !xrtValueGetBytes(key, &view)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake3", 0,
            "blake3 key must be bytes");
        return NULL;
    }
    return xblake3_hasher_create_keyed((void*)view.Data, view.Size, maximum);
}
XBLAKE3_EXPORT void* xblake3_hasher_create_kdf(const char* context,
    size_t context_len, unsigned long long maximum)
{
    xrtblake3options options;
    xrtBlake3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtBlake3HasherCreateDeriveKey(&options, context, context_len);
}
XBLAKE3_EXPORT void xblake3_hasher_destroy(void* handle) { xrtBlake3HasherDestroy(handle); }
XBLAKE3_EXPORT bool xblake3_hasher_update(void* handle, void* data, size_t size)
{ return xrtBlake3HasherUpdate(handle, data, size); }
XBLAKE3_EXPORT bool xblake3_hasher_update_str(void* handle, const char* data, size_t size)
{ return xblake3_hasher_update(handle, (void*)data, size); }
XBLAKE3_EXPORT bool xblake3_hasher_reset(void* handle) { return xrtBlake3HasherReset(handle); }
XBLAKE3_EXPORT unsigned long long xblake3_hasher_input_bytes(void* handle)
{ return (unsigned long long)xrtBlake3HasherInputBytes(handle); }
XBLAKE3_EXPORT void* xblake3_hasher_adopt(void* handle) { return handle; }
XBLAKE3_EXPORT void* xblake3_hasher_clone(void* handle) { return xrtBlake3HasherClone(handle); }

XBLAKE3_EXPORT const char* xblake3_hasher_digest_hex(void* handle, size_t outlen)
{
    uint8* digest;
    char* text;
    xrtClearError();
    text = blake3_hex_space(outlen, &digest);
    if (text == NULL || !xrtBlake3HasherDigest(handle, digest, outlen)) return "";
    return blake3_hex_finish(digest, outlen, text);
}
XBLAKE3_EXPORT char* xblake3_hasher_digest_hex_owned(void* handle, size_t outlen)
{
    uint8* digest;
    xrtClearError();
    if (!blake3_size(outlen)) return NULL;
    digest = xrtTemp(outlen);
    if (digest == NULL) return NULL;
    if (!xrtBlake3HasherDigest(handle, digest, outlen)) {
        xrtSecureZero(digest, outlen);
        return NULL;
    }
    return blake3_hex_owned(digest, outlen);
}
XBLAKE3_EXPORT void* xblake3_hasher_digest_bytes(void* handle, size_t outlen)
{
    uint8* digest;
    xbuffer* buffer;
    xrtClearError();
    buffer = blake3_buffer(outlen, &digest);
    if (buffer == NULL) return NULL;
    if (!xrtBlake3HasherDigest(handle, digest, outlen)) {
        xrtBufferDestroy(buffer);
        return NULL;
    }
    return buffer;
}

XBLAKE3_EXPORT void* xblake3_hasher_create_keyed_span(xbytesview key,
    unsigned long long maximum)
{
    xrtClearError();
    return xblake3_hasher_create_keyed((void*)key.Data, key.Size, maximum);
}
