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
#include "include/xrt_sha3.h"
#include "include/sha3_native.h"

bool xsha3_available(void) { return true; }
const char* xsha3_version(void) { return "1.0.0"; }
const char* xsha3_last_error(void) { return xrtErrorMessage(xrtGetError()); }

// ----------------------------- 分配型输出与所有权 -----------------------------

static bool sha3_space_size(size_t size)
{
    xrtresourcelimits limits;
    xrtResourceLimitsInit(&limits);
    if (size > (SIZE_MAX - 1) / 3 || (limits.iMaxOutputBytes != 0 && size > limits.iMaxOutputBytes)) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.sha3", 0, "sha3 output exceeds allocation limit");
        return false;
    }
    return true;
}
static char* sha3_hex_space(size_t size, uint8** digest)
{
    if (!sha3_space_size(size)) return NULL;
    *digest = xrtTemp(size * 3 + 1);
    return *digest != NULL ? (char*)*digest + size : NULL;
}
static const char* sha3_hex_finish(const uint8* digest, size_t size, char* text)
{
    size_t written;
    return xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0) ? text : "";
}
static char* sha3_hex_owned_space(size_t size, uint8** digest)
{
    char* text;
    if (!sha3_space_size(size)) return NULL;
    text = (char*)xrtMalloc(size * 3 + 1);
    *digest = text != NULL ? (uint8*)text + size * 2 + 1 : NULL;
    return text;
}
static void* sha3_hex_owned_finish(char* text, const uint8* digest, size_t size, bool ok)
{
    size_t written;
    xerror* error;
    if (ok && xrtHexEncode(digest, size, text, size * 2 + 1, &written, 0)) return text;
    error = xrtTakeError();
    xrtFree(text);
    xrtSetErrorTake(error);
    return NULL;
}
static xbuffer* sha3_buffer(size_t size, uint8** digest)
{
    xbuffer* buffer;
    *digest = NULL;
    if (!sha3_space_size(size)) return NULL;
    buffer = xrtBufferCreate();
    if (buffer == NULL) return NULL;
    if (size != 0) {
        *digest = xrtBufferAdd(buffer, size);
        if (*digest == NULL) {
            xerror* error = xrtTakeError();
            xrtBufferDestroy(buffer);
            xrtSetErrorTake(error);
            return NULL;
        }
    }
    return buffer;
}
static void* sha3_buffer_finish(xbuffer* buffer, bool ok)
{
    if (!ok) {
        xerror* error = xrtTakeError();
        xrtBufferDestroy(buffer);
        xrtSetErrorTake(error);
        return NULL;
    }
    return buffer;
}
static bool sha3_parameters(const void* data, size_t size, int bits, bool shake, size_t* length)
{
    xrtClearError();
    if (data == NULL && size != 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sha3", 0, "invalid sha3 input");
        return false;
    }
    if (!shake) { *length = xrtSha3DigestSize(bits); return *length != 0; }
    if (bits != 128 && bits != 256) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sha3", 0, "invalid shake strength");
        return false;
    }
    return true;
}

// ----------------------------- 文本与二进制一次性接口 -----------------------------

static const char* sha3_hex_once(const void* data, size_t size, int bits, bool shake, size_t length)
{
    uint8* digest;
    char* text;
    bool ok;
    if (!sha3_parameters(data, size, bits, shake, &length)) return "";
    text = sha3_hex_space(length, &digest);
    if (text == NULL) return "";
    ok = shake ? xrtShakeHash(data, size, bits, digest, length, NULL) :
                 xrtSha3Hash(data, size, bits, digest, length, NULL);
    return ok ? sha3_hex_finish(digest, length, text) : "";
}
static void* sha3_hex_once_owned(const void* data, size_t size, int bits, bool shake, size_t length)
{
    uint8* digest;
    char* text;
    bool ok;
    if (!sha3_parameters(data, size, bits, shake, &length)) return NULL;
    text = sha3_hex_owned_space(length, &digest);
    if (text == NULL) return NULL;
    ok = shake ? xrtShakeHash(data, size, bits, digest, length, NULL) :
                 xrtSha3Hash(data, size, bits, digest, length, NULL);
    return sha3_hex_owned_finish(text, digest, length, ok);
}
static void* sha3_bytes_once(const void* data, size_t size, int bits, bool shake, size_t length)
{
    uint8* digest;
    xbuffer* buffer;
    bool ok;
    if (!sha3_parameters(data, size, bits, shake, &length)) return NULL;
    buffer = sha3_buffer(length, &digest);
    if (buffer == NULL) return NULL;
    ok = shake ? xrtShakeHash(data, size, bits, digest, length, NULL) :
                 xrtSha3Hash(data, size, bits, digest, length, NULL);
    return sha3_buffer_finish(buffer, ok);
}
const char* xsha3_hash_hex(void* data, size_t size, int bits)
{ return sha3_hex_once(data, size, bits, false, 0); }
const char* xsha3_hash_str_hex(const char* data, size_t size, int bits)
{ return sha3_hex_once(data, size, bits, false, 0); }
void* xsha3_hash_hex_owned(void* data, size_t size, int bits)
{ return sha3_hex_once_owned(data, size, bits, false, 0); }
void* xsha3_hash_str_hex_owned(const char* data, size_t size, int bits)
{ return sha3_hex_once_owned(data, size, bits, false, 0); }
void* xsha3_hash_bytes(void* data, size_t size, int bits)
{ return sha3_bytes_once(data, size, bits, false, 0); }
void* xsha3_hash_str_bytes(const char* data, size_t size, int bits)
{ return sha3_bytes_once(data, size, bits, false, 0); }
const char* xshake_hash_hex(void* data, size_t size, int strength, size_t length)
{ return sha3_hex_once(data, size, strength, true, length); }
const char* xshake_hash_str_hex(const char* data, size_t size, int strength, size_t length)
{ return sha3_hex_once(data, size, strength, true, length); }
void* xshake_hash_hex_owned(void* data, size_t size, int strength, size_t length)
{ return sha3_hex_once_owned(data, size, strength, true, length); }
void* xshake_hash_str_hex_owned(const char* data, size_t size, int strength, size_t length)
{ return sha3_hex_once_owned(data, size, strength, true, length); }
void* xshake_hash_bytes(void* data, size_t size, int strength, size_t length)
{ return sha3_bytes_once(data, size, strength, true, length); }
void* xshake_hash_str_bytes(const char* data, size_t size, int strength, size_t length)
{ return sha3_bytes_once(data, size, strength, true, length); }

// ----------------------------- 文件与同步回调 -----------------------------

static bool sha3_path(const char* path, uint64 maximum, int bits, xvalue* callback, uint8* digest, size_t length)
{
    xrtresourcelimits limits;
    xrtprogresscall progress;
    xreader* reader;
    xerror* failure;
    bool ok, closed;
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sha3", 0, "sha3 file path is required");
        return false;
    }
    reader = xrtReaderOpen(path);
    if (reader == NULL) return false;
    xrtResourceLimitsInit(&limits);
    if (maximum != 0) limits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    ok = xrtSha3File(reader, &limits, bits, digest, length,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    failure = !ok ? xrtTakeError() : NULL;
    closed = xrtReaderDestroy(reader);
    if (!ok) xrtSetErrorTake(failure);
    return ok && closed;
}
const char* xsha3_file_hex(const char* path, unsigned long long maximum, int bits, xvalue* callback)
{
    uint8 digest[XRT_SHA3_MAX_OUT];
    uint8* temporary;
    char* text;
    size_t length = xrtSha3DigestSize(bits);
    if (length == 0 || !sha3_path(path, maximum, bits, callback, digest, length)) return "";
    /* 不跨用户回调借用临时区，回调完成后才创建返回文本。 */
    text = sha3_hex_space(length, &temporary);
    return text != NULL ? sha3_hex_finish(digest, length, text) : "";
}
void* xsha3_file_bytes(const char* path, unsigned long long maximum, int bits, xvalue* callback)
{
    uint8 digest[XRT_SHA3_MAX_OUT];
    xbuffer* buffer;
    size_t length = xrtSha3DigestSize(bits);
    if (length == 0 || !sha3_path(path, maximum, bits, callback, digest, length)) return NULL;
    buffer = xrtBufferCreate();
    if (buffer == NULL) return NULL;
    return sha3_buffer_finish(buffer, xrtBufferAssign(buffer, (xbytesview){digest, length}));
}

// ----------------------------- 流式对象薄绑定 -----------------------------

void* xsha3_hasher_create(int bits, unsigned long long maximum)
{
    xrtsha3options options;
    xrtSha3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtSha3HasherCreate(&options, bits);
}
void* xsha3_shake_hasher_create(int strength, unsigned long long maximum)
{
    xrtsha3options options;
    xrtSha3OptionsInit(&options);
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtSha3ShakeHasherCreate(&options, strength);
}
void xsha3_hasher_destroy(void* handle) { xrtSha3HasherDestroy(handle); }
bool xsha3_hasher_update(void* handle, void* data, size_t size)
{ return xrtSha3HasherUpdate(handle, data, size); }
bool xsha3_hasher_update_str(void* handle, const char* data, size_t size)
{ return xrtSha3HasherUpdate(handle, data, size); }
const char* xsha3_hasher_digest_hex(void* handle, size_t length)
{
    uint8* digest;
    char* text;
    if (!xrtSha3HasherCheckOutput(handle, length)) return "";
    text = sha3_hex_space(length, &digest);
    return text != NULL && xrtSha3HasherDigest(handle, digest, length) ?
        sha3_hex_finish(digest, length, text) : "";
}
void* xsha3_hasher_digest_hex_owned(void* handle, size_t length)
{
    uint8* digest;
    char* text;
    if (!xrtSha3HasherCheckOutput(handle, length)) return NULL;
    text = sha3_hex_owned_space(length, &digest);
    if (text == NULL) return NULL;
    return sha3_hex_owned_finish(text, digest, length,
        xrtSha3HasherDigest(handle, digest, length));
}
void* xsha3_hasher_digest_bytes(void* handle, size_t length)
{
    uint8* digest;
    xbuffer* buffer;
    if (!xrtSha3HasherCheckOutput(handle, length)) return NULL;
    buffer = sha3_buffer(length, &digest);
    if (buffer == NULL) return NULL;
    return sha3_buffer_finish(buffer, xrtSha3HasherDigest(handle, digest, length));
}
bool xsha3_hasher_reset(void* handle) { return xrtSha3HasherReset(handle); }
unsigned long long xsha3_hasher_input_bytes(void* handle) { return xrtSha3HasherInputBytes(handle); }
int xsha3_hasher_out_bytes(void* handle) { return xrtSha3HasherOutBytes(handle); }
void* xsha3_hasher_adopt(void* handle)
{
    xrtClearError();
    if (handle == NULL) xrtSetErrorInfo(XERR_STATE, "xlang.extlib.sha3", 0, "invalid sha3 handle");
    return handle;
}
void* xsha3_hasher_clone(void* handle) { return xrtSha3HasherClone(handle); }
