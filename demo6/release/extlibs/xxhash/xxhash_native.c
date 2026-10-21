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
#include "include/xrt_xxhash.h"
#include "include/xxhash_native.h"
#include "third_party/xxhash.h"
#include <stdio.h>

/* 固定版本字符串无首次调用写入竞争；数值摘要完整保留无符号位。 */
#define XXXHASH_STRING_INNER(x) #x
#define XXXHASH_STRING(x) XXXHASH_STRING_INNER(x)
bool xxxhash_available(void) { return true; }
const char* xxxhash_version(void)
{
    return XXXHASH_STRING(XXH_VERSION_MAJOR) "." XXXHASH_STRING(XXH_VERSION_MINOR) "." XXXHASH_STRING(XXH_VERSION_RELEASE);
}
const char* xxxhash_last_error(void) { return xrtErrorMessage(xrtGetError()); }

/* 文本借用当前 XRT 临时区，不被后续 hash 或跨线程调用覆盖。 */
static const char* xxhash_hex(uint64 value, bool wide)
{
    char* text = xrtTemp(wide ? 17 : 9);
    if (text == NULL) return "";
    if (wide) snprintf(text, 17, "%016llx", (unsigned long long)value);
    else snprintf(text, 9, "%08x", (unsigned int)value);
    return text;
}

unsigned long long xxxhash_hash64(void* data, size_t size, unsigned long long seed)
{
    uint64 hash = 0;
    xrtXxhash64(data, size, seed, &hash, NULL);
    return (unsigned long long)hash;
}
unsigned long long xxxhash_hash64_str(const char* data, size_t size, unsigned long long seed)
{ return xxxhash_hash64((void*)data, size, seed); }
const char* xxxhash_hash64_hex(void* data, size_t size, unsigned long long seed)
{
    uint64 hash;
    return xrtXxhash64(data, size, seed, &hash, NULL) ? xxhash_hex(hash, true) : "";
}
const char* xxxhash_hash64_str_hex(const char* data, size_t size, unsigned long long seed)
{ return xxxhash_hash64_hex((void*)data, size, seed); }
unsigned int xxxhash_hash32(void* data, size_t size, unsigned int seed)
{
    uint32 hash = 0;
    xrtXxhash32(data, size, seed, &hash, NULL);
    return (unsigned int)hash;
}
unsigned int xxxhash_hash32_str(const char* data, size_t size, unsigned int seed)
{ return xxxhash_hash32((void*)data, size, seed); }
const char* xxxhash_hash32_hex(void* data, size_t size, unsigned int seed)
{
    uint32 hash;
    return xrtXxhash32(data, size, seed, &hash, NULL) ? xxhash_hex(hash, false) : "";
}
const char* xxxhash_hash32_str_hex(const char* data, size_t size, unsigned int seed)
{ return xxxhash_hash32_hex((void*)data, size, seed); }

// -------------------------------- 文件与回调 --------------------------------

static bool xxhash_file(const char* path, uint64 seed, uint64 maximum, xvalue* callback, uint64* hash)
{
    xrtresourcelimits limits;
    xrtprogresscall progress;
    xreader* reader;
    bool ok;
    bool closed;
    xerror* failure;
    xrtClearError();
    if (path == NULL || path[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xxhash", 0, "xxhash file path is required");
        return false;
    }
    reader = xrtReaderOpen(path);
    if (reader == NULL) return false;
    xrtXxhashLimitsInit(&limits);
    if (maximum != 0) limits.iMaxInputBytes = maximum;
    xrtProgressCallInit(&progress, callback);
    ok = xrtXxhashFile64(reader, seed, &limits, hash,
        progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, NULL);
    failure = !ok ? xrtTakeError() : NULL;
    closed = xrtReaderDestroy(reader);
    if (!ok) xrtSetErrorTake(failure);
    return ok && closed;
}

unsigned long long xxxhash_file64(const char* path, unsigned long long seed,
    unsigned long long maximum, xvalue* callback)
{
    uint64 hash = 0;
    return xxhash_file(path, seed, maximum, callback, &hash) ? (unsigned long long)hash : 0;
}
const char* xxxhash_file64_hex(const char* path, unsigned long long seed,
    unsigned long long maximum, xvalue* callback)
{
    uint64 hash;
    return xxhash_file(path, seed, maximum, callback, &hash) ? xxhash_hex(hash, true) : "";
}

// -------------------------------- 流式对象薄绑定 --------------------------------

void* xxxhash_hasher_create(unsigned long long seed, unsigned long long maximum)
{
    xrtxxhashoptions options;
    xrtXxhashOptionsInit(&options);
    options.iSeed = seed;
    if (maximum != 0) options.tLimits.iMaxInputBytes = maximum;
    return xrtXxhashHasherCreate(&options);
}
void xxxhash_hasher_destroy(void* handle) { xrtXxhashHasherDestroy(handle); }
bool xxxhash_hasher_update(void* handle, void* data, size_t size)
{ return xrtXxhashHasherUpdate(handle, data, size); }
bool xxxhash_hasher_update_str(void* handle, const char* data, size_t size)
{ return xrtXxhashHasherUpdate(handle, data, size); }
unsigned long long xxxhash_hasher_digest(void* handle)
{
    uint64 hash = 0;
    xrtXxhashHasherDigest(handle, &hash);
    return (unsigned long long)hash;
}
const char* xxxhash_hasher_digest_hex(void* handle)
{
    uint64 hash;
    return xrtXxhashHasherDigest(handle, &hash) ? xxhash_hex(hash, true) : "";
}
bool xxxhash_hasher_reset(void* handle) { return xrtXxhashHasherReset(handle); }
unsigned long long xxxhash_hasher_input_bytes(void* handle)
{ return (unsigned long long)xrtXxhashHasherInputBytes(handle); }
