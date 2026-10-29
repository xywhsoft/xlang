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
#include "include/xrt_blake2.h"
#include "include/blake2_native.h"

#if defined(_WIN32)
#define XBLAKE2_EXPORT __declspec(dllexport)
#else
#define XBLAKE2_EXPORT
#endif

/* 类型错误、限额、IO 和取消不再折叠成同一个 ValueError。 */
static void blake2_publish(const xrtblake2error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    if (error != NULL) {
        switch (error->iCode) {
        case XRT_BLAKE2_ERROR_MEMORY: kind = XERR_MEMORY; break;
        case XRT_BLAKE2_ERROR_IO: kind = XERR_IO; break;
        case XRT_BLAKE2_ERROR_LIMIT: kind = XERR_RANGE; break;
        case XRT_BLAKE2_ERROR_STATE: kind = XERR_STATE; break;
        case XRT_BLAKE2_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
        default: break;
        }
    }
    xrtSetErrorInfo(kind, "xlang.extlib.blake2", 0,
        error != NULL && error->sMessage[0] ? error->sMessage : "invalid blake2 handle");
}

/* XRT 临时区负责线程/协程隔离；不使用在 TCC 下退化成共享全局的环形字符串槽。 */
static const char* blake2_hex(uint8* digest, size_t size)
{
    size_t written;
    char* text = xrtTemp(size * 2 + 1);
    bool ok = text != NULL && xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0);
    xrtSecureZero(digest, size);
    return ok ? text : "";
}

static void* blake2_buffer(uint8* digest, size_t size)
{
    xbuffer* buffer = xrtBufferFrom((xbytesview){digest, size});
    xrtSecureZero(digest, size);
    return buffer;
}

/* 文件读取和关闭均成功才发布摘要；回调异常优先于关闭错误。 */
static bool blake2_file_complete(xreader* reader, bool ok,
    bool callback_failed, const xrtblake2error* error)
{
    xerror* first = !ok && callback_failed ? xrtTakeError() : NULL;
    bool closed = xrtReaderDestroy(reader);
    if (!ok) {
        if (first != NULL) xrtSetErrorTake(first);
        else blake2_publish(error);
        return false;
    }
    return closed;
}

XBLAKE2_EXPORT bool xblake2_available(void) { return true; }
XBLAKE2_EXPORT const char* xblake2_version(void) { return "1.1.0"; }
XBLAKE2_EXPORT const char* xblake2_last_error(void) { return xrtErrorMessage(xrtGetError()); }

/* BLAKE2b 薄绑定：point 对应 void*，只在进入 const 核心接口时借用。 */
XBLAKE2_EXPORT const char* xblake2b_hash_hex(void* data, size_t size,
    size_t outlen, void* key, size_t keylen)
{
    uint8 digest[64];
    xrtblake2error error;
    xrtClearError();
    if (!xrtBlake2bHash(data, size, outlen, key, keylen, digest, &error)) {
        blake2_publish(&error);
        return "";
    }
    return blake2_hex(digest, outlen);
}

XBLAKE2_EXPORT const char* xblake2b_hash_str_hex(const char* data, size_t size,
    size_t outlen, const char* key, size_t keylen)
{
    return xblake2b_hash_hex((void*)data, size, outlen, (void*)key, keylen);
}

XBLAKE2_EXPORT void* xblake2b_hash_mac_bytes(void* data, size_t size,
    void* key, size_t keylen, size_t outlen)
{
    uint8 digest[64];
    xrtblake2error error;
    xrtClearError();
    if (!xrtBlake2bHash(data, size, outlen, key, keylen, digest, &error)) {
        blake2_publish(&error);
        return NULL;
    }
    return blake2_buffer(digest, outlen);
}

XBLAKE2_EXPORT void* xblake2b_hash_bytes(void* data, size_t size, size_t outlen)
{
    return xblake2b_hash_mac_bytes(data, size, NULL, 0, outlen);
}

XBLAKE2_EXPORT void* xblake2b_hash_str_bytes(const char* data, size_t size, size_t outlen)
{
    return xblake2b_hash_bytes((void*)data, size, outlen);
}

XBLAKE2_EXPORT void* xblake2b_hash_mac_str_bytes(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen)
{
    return xblake2b_hash_mac_bytes((void*)data, size, (void*)key, keylen, outlen);
}

/* 每次调用用局部状态，允许回调内执行另一次独立的文件哈希。 */
XBLAKE2_EXPORT const char* xblake2b_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback)
{
    xrtblake2options options;
    xrtblake2error error;
    xrtprogresscall progress;
    xreader* reader;
    uint8 digest[64];
    bool ok;
    xrtClearError();
    if (path == NULL || path[0] == 0 || outlen == 0 || outlen > 64) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake2", 0, "invalid blake2 file path or digest size");
        return "";
    }
    xrtBlake2OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    reader = xrtReaderOpen(path);
    if (reader == NULL) return "";
    ok = xrtBlake2bFile(reader, &options.tLimits, outlen, digest,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, &error);
    if (!blake2_file_complete(reader, ok, progress.InvokeFailed, &error)) {
        xrtSecureZero(digest, sizeof(digest));
        return "";
    }
    return blake2_hex(digest, outlen);
}

XBLAKE2_EXPORT void* xblake2b_hasher_create_keyed(size_t outlen, void* key,
    size_t keylen, unsigned long long maximum)
{
    xrtblake2options options;
    xrtBlake2OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtBlake2bHasherCreate(&options, outlen, key, keylen);
}

/* 从 bytes 值读取真实密钥长度，避免语言侧伪造裸指针长度。 */
XBLAKE2_EXPORT void* xblake2b_hasher_create_keyed_bytes(size_t outlen,
    xvalue* key, unsigned long long maximum)
{
    xbytesview view = {0};
    xrtClearError();
    if (key == NULL || !xrtValueGetBytes(key, &view)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake2", 0,
            "blake2 key must be bytes");
        return NULL;
    }
    return xblake2b_hasher_create_keyed(outlen, (void*)view.Data,
        view.Size, maximum);
}

XBLAKE2_EXPORT void* xblake2b_hasher_create(size_t outlen, unsigned long long maximum)
{
    return xblake2b_hasher_create_keyed(outlen, NULL, 0, maximum);
}

XBLAKE2_EXPORT void xblake2b_hasher_destroy(void* handle)
{
    xrtBlake2bHasherDestroy(handle);
}

XBLAKE2_EXPORT bool xblake2b_hasher_update(void* handle, void* data, size_t size)
{
    xrtClearError();
    if (!xrtBlake2bHasherUpdate(handle, data, size)) {
        blake2_publish(xrtBlake2bHasherError(handle));
        return false;
    }
    return true;
}

XBLAKE2_EXPORT bool xblake2b_hasher_update_str(void* handle, const char* text, size_t size)
{
    return xblake2b_hasher_update(handle, (void*)text, size);
}

XBLAKE2_EXPORT const char* xblake2b_hasher_digest_hex(void* handle)
{
    uint8 digest[64];
    size_t size = xrtBlake2bHasherDigestSize(handle);
    xrtClearError();
    if (!xrtBlake2bHasherDigest(handle, digest, size)) {
        blake2_publish(xrtBlake2bHasherError(handle));
        return "";
    }
    return blake2_hex(digest, size);
}

XBLAKE2_EXPORT void* xblake2b_hasher_digest_bytes(void* handle)
{
    uint8 digest[64];
    size_t size = xrtBlake2bHasherDigestSize(handle);
    xrtClearError();
    if (!xrtBlake2bHasherDigest(handle, digest, size)) {
        blake2_publish(xrtBlake2bHasherError(handle));
        return NULL;
    }
    return blake2_buffer(digest, size);
}

XBLAKE2_EXPORT bool xblake2b_hasher_reset(void* handle)
{
    xrtClearError();
    if (!xrtBlake2bHasherReset(handle)) {
        blake2_publish(NULL);
        return false;
    }
    return true;
}

XBLAKE2_EXPORT unsigned long long xblake2b_hasher_input_bytes(void* handle)
{
    return (unsigned long long)xrtBlake2bHasherInputBytes(handle);
}

XBLAKE2_EXPORT void* xblake2b_hasher_adopt(void* handle) { return handle; }

XBLAKE2_EXPORT void* xblake2b_hasher_clone(void* handle)
{
    xrtClearError();
    return xrtBlake2bHasherClone(handle);
}

/* BLAKE2s 薄绑定：point 对应 void*，只在进入 const 核心接口时借用。 */
XBLAKE2_EXPORT const char* xblake2s_hash_hex(void* data, size_t size,
    size_t outlen, void* key, size_t keylen)
{
    uint8 digest[32];
    xrtblake2error error;
    xrtClearError();
    if (!xrtBlake2sHash(data, size, outlen, key, keylen, digest, &error)) {
        blake2_publish(&error);
        return "";
    }
    return blake2_hex(digest, outlen);
}

XBLAKE2_EXPORT const char* xblake2s_hash_str_hex(const char* data, size_t size,
    size_t outlen, const char* key, size_t keylen)
{
    return xblake2s_hash_hex((void*)data, size, outlen, (void*)key, keylen);
}

XBLAKE2_EXPORT void* xblake2s_hash_mac_bytes(void* data, size_t size,
    void* key, size_t keylen, size_t outlen)
{
    uint8 digest[32];
    xrtblake2error error;
    xrtClearError();
    if (!xrtBlake2sHash(data, size, outlen, key, keylen, digest, &error)) {
        blake2_publish(&error);
        return NULL;
    }
    return blake2_buffer(digest, outlen);
}

XBLAKE2_EXPORT void* xblake2s_hash_bytes(void* data, size_t size, size_t outlen)
{
    return xblake2s_hash_mac_bytes(data, size, NULL, 0, outlen);
}

XBLAKE2_EXPORT void* xblake2s_hash_str_bytes(const char* data, size_t size, size_t outlen)
{
    return xblake2s_hash_bytes((void*)data, size, outlen);
}

XBLAKE2_EXPORT void* xblake2s_hash_mac_str_bytes(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen)
{
    return xblake2s_hash_mac_bytes((void*)data, size, (void*)key, keylen, outlen);
}

/* 每次调用用局部状态，允许回调内执行另一次独立的文件哈希。 */
XBLAKE2_EXPORT const char* xblake2s_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback)
{
    xrtblake2options options;
    xrtblake2error error;
    xrtprogresscall progress;
    xreader* reader;
    uint8 digest[32];
    bool ok;
    xrtClearError();
    if (path == NULL || path[0] == 0 || outlen == 0 || outlen > 32) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake2", 0, "invalid blake2 file path or digest size");
        return "";
    }
    xrtBlake2OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    reader = xrtReaderOpen(path);
    if (reader == NULL) return "";
    ok = xrtBlake2sFile(reader, &options.tLimits, outlen, digest,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, &error);
    if (!blake2_file_complete(reader, ok, progress.InvokeFailed, &error)) {
        xrtSecureZero(digest, sizeof(digest));
        return "";
    }
    return blake2_hex(digest, outlen);
}

XBLAKE2_EXPORT void* xblake2s_hasher_create_keyed(size_t outlen, void* key,
    size_t keylen, unsigned long long maximum)
{
    xrtblake2options options;
    xrtBlake2OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtBlake2sHasherCreate(&options, outlen, key, keylen);
}

XBLAKE2_EXPORT void* xblake2s_hasher_create_keyed_bytes(size_t outlen,
    xvalue* key, unsigned long long maximum)
{
    xbytesview view = {0};
    xrtClearError();
    if (key == NULL || !xrtValueGetBytes(key, &view)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.blake2", 0,
            "blake2 key must be bytes");
        return NULL;
    }
    return xblake2s_hasher_create_keyed(outlen, (void*)view.Data,
        view.Size, maximum);
}

XBLAKE2_EXPORT void* xblake2s_hasher_create(size_t outlen, unsigned long long maximum)
{
    return xblake2s_hasher_create_keyed(outlen, NULL, 0, maximum);
}

XBLAKE2_EXPORT void xblake2s_hasher_destroy(void* handle)
{
    xrtBlake2sHasherDestroy(handle);
}

XBLAKE2_EXPORT bool xblake2s_hasher_update(void* handle, void* data, size_t size)
{
    xrtClearError();
    if (!xrtBlake2sHasherUpdate(handle, data, size)) {
        blake2_publish(xrtBlake2sHasherError(handle));
        return false;
    }
    return true;
}

XBLAKE2_EXPORT bool xblake2s_hasher_update_str(void* handle, const char* text, size_t size)
{
    return xblake2s_hasher_update(handle, (void*)text, size);
}

XBLAKE2_EXPORT const char* xblake2s_hasher_digest_hex(void* handle)
{
    uint8 digest[32];
    size_t size = xrtBlake2sHasherDigestSize(handle);
    xrtClearError();
    if (!xrtBlake2sHasherDigest(handle, digest, size)) {
        blake2_publish(xrtBlake2sHasherError(handle));
        return "";
    }
    return blake2_hex(digest, size);
}

XBLAKE2_EXPORT void* xblake2s_hasher_digest_bytes(void* handle)
{
    uint8 digest[32];
    size_t size = xrtBlake2sHasherDigestSize(handle);
    xrtClearError();
    if (!xrtBlake2sHasherDigest(handle, digest, size)) {
        blake2_publish(xrtBlake2sHasherError(handle));
        return NULL;
    }
    return blake2_buffer(digest, size);
}

XBLAKE2_EXPORT bool xblake2s_hasher_reset(void* handle)
{
    xrtClearError();
    if (!xrtBlake2sHasherReset(handle)) {
        blake2_publish(NULL);
        return false;
    }
    return true;
}

XBLAKE2_EXPORT unsigned long long xblake2s_hasher_input_bytes(void* handle)
{
    return (unsigned long long)xrtBlake2sHasherInputBytes(handle);
}

XBLAKE2_EXPORT void* xblake2s_hasher_adopt(void* handle) { return handle; }

XBLAKE2_EXPORT void* xblake2s_hasher_clone(void* handle)
{
    xrtClearError();
    return xrtBlake2sHasherClone(handle);
}

XBLAKE2_EXPORT void* xblake2b_hasher_create_keyed_span(size_t outlen,
    xbytesview key, unsigned long long maximum)
{
    xrtClearError();
    return xblake2b_hasher_create_keyed(outlen, (void*)key.Data, key.Size, maximum);
}

XBLAKE2_EXPORT void* xblake2s_hasher_create_keyed_span(size_t outlen,
    xbytesview key, unsigned long long maximum)
{
    xrtClearError();
    return xblake2s_hasher_create_keyed(outlen, (void*)key.Data, key.Size, maximum);
}
