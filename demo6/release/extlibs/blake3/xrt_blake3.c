#include "include/xrt_blake3.h"
#include "third_party/blake3.h"
#include <stdio.h>
#include <string.h>

#define XRT_BLAKE3_CHUNK 65536u

struct xrtblake3hasher {
    blake3_hasher Hasher;
    xrtblake3options Options;
    xrtblake3error Error;
    uint64 InputBytes;
    bool Failed;
};

/* 独立 C 和语言薄绑定使用同一错误分类。 */
static bool blake3_publish(const xrtblake3error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_BLAKE3_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_BLAKE3_ERROR_IO: kind = XERR_IO; break;
    case XRT_BLAKE3_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_BLAKE3_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_BLAKE3_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.blake3", error->iCode, error->sMessage);
    return false;
}

static bool blake3_fail(xrtblake3error* error, int code, uint64 input, const char* message)
{
    xrtblake3error local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = input;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return blake3_publish(error);
}

static bool blake3_limits(const xrtresourcelimits* limits)
{
    return limits->iSize == sizeof(*limits) &&
        limits->iVersion == XRT_RESOURCE_LIMITS_VERSION &&
        limits->iMaxItemBytes == 0 && limits->iMaxEntries == 0 &&
        limits->iMaxNodes == 0 && limits->iMaxDepth == 0 &&
        limits->iMaxCompressionRatio == 0 && limits->iFlags == 0 &&
        limits->iReserved == 0;
}

static bool blake3_input(uint64 current, uint64 add, uint64 maximum, xrtblake3error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return blake3_fail(error, XRT_BLAKE3_ERROR_LIMIT, current, "blake3 input exceeds maxInput limit");
    return true;
}

XRT_API void xrtBlake3ErrorInit(xrtblake3error* error)
{
    if (error != NULL) memset(error, 0, sizeof(*error));
}

XRT_API void xrtBlake3LimitsInit(xrtresourcelimits* limits)
{
    if (limits == NULL) return;
    xrtResourceLimitsInit(limits);
    limits->iMaxItemBytes = 0;
    limits->iMaxEntries = 0;
    limits->iMaxNodes = 0;
    limits->iMaxDepth = 0;
    limits->iMaxCompressionRatio = 0;
}

XRT_API void xrtBlake3OptionsInit(xrtblake3options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_BLAKE3_OPTIONS_VERSION;
    xrtBlake3LimitsInit(&options->tLimits);
}

static xrtblake3hasher* blake3_create(const xrtblake3options* options,
    const uint8* key, const void* context, size_t context_len, int mode)
{
    xrtblake3hasher* h;
    xrtClearError();
    if ((options != NULL && (options->iSize != sizeof(*options) ||
        options->iVersion != XRT_BLAKE3_OPTIONS_VERSION || !blake3_limits(&options->tLimits))) ||
        (mode == 1 && key == NULL) || (mode == 2 && context == NULL)) {
        blake3_fail(NULL, XRT_BLAKE3_ERROR_ARGUMENT, 0, "invalid blake3 options, key or context");
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtBlake3OptionsInit(&h->Options);
    if (mode == 1) blake3_hasher_init_keyed(&h->Hasher, key);
    else if (mode == 2) blake3_hasher_init_derive_key_raw(&h->Hasher, context, context_len);
    else blake3_hasher_init(&h->Hasher);
    return h;
}

XRT_API xrtblake3hasher* xrtBlake3HasherCreate(const xrtblake3options* options)
{ return blake3_create(options, NULL, NULL, 0, 0); }
XRT_API xrtblake3hasher* xrtBlake3HasherCreateKeyed(const xrtblake3options* options,
    const uint8 key[XRT_BLAKE3_KEY_LEN])
{ return blake3_create(options, key, NULL, 0, 1); }
XRT_API xrtblake3hasher* xrtBlake3HasherCreateDeriveKey(const xrtblake3options* options,
    const void* context, size_t context_len)
{ return blake3_create(options, NULL, context, context_len, 2); }

static bool blake3_ready(xrtblake3hasher* h)
{
    if (h == NULL) return blake3_fail(NULL, XRT_BLAKE3_ERROR_STATE, 0, "invalid blake3 handle");
    if (h->Failed) return blake3_publish(&h->Error);
    xrtBlake3ErrorInit(&h->Error);
    return true;
}

XRT_API bool xrtBlake3HasherUpdate(xrtblake3hasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!blake3_ready(h)) return false;
    if (size != 0 && data == NULL) {
        h->Failed = true;
        return blake3_fail(&h->Error, XRT_BLAKE3_ERROR_ARGUMENT, h->InputBytes, "invalid blake3 input");
    }
    if (!blake3_input(h->InputBytes, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    if (size != 0) blake3_hasher_update(&h->Hasher, data, size);
    h->InputBytes += size;
    return true;
}

XRT_API bool xrtBlake3HasherDigest(xrtblake3hasher* h, uint8* output, size_t size)
{
    xrtClearError();
    if (!blake3_ready(h)) return false;
    if (output == NULL || size == 0)
        return blake3_fail(&h->Error, XRT_BLAKE3_ERROR_ARGUMENT, h->InputBytes, "invalid blake3 output");
    if (h->Options.tLimits.iMaxOutputBytes != 0 && size > h->Options.tLimits.iMaxOutputBytes)
        return blake3_fail(&h->Error, XRT_BLAKE3_ERROR_LIMIT, h->InputBytes, "blake3 output exceeds maxOutput limit");
    blake3_hasher_finalize(&h->Hasher, output, size);
    return true;
}

/* 上游 reset 保留模式和派生后的 key，不保存或截断原始上下文。 */
XRT_API bool xrtBlake3HasherReset(xrtblake3hasher* h)
{
    xrtClearError();
    if (h == NULL) return blake3_fail(NULL, XRT_BLAKE3_ERROR_STATE, 0, "invalid blake3 handle");
    blake3_hasher_reset(&h->Hasher);
    xrtSecureZero(h->Hasher.cv_stack, sizeof(h->Hasher.cv_stack));
    h->InputBytes = 0;
    h->Failed = false;
    xrtBlake3ErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtBlake3HasherInputBytes(const xrtblake3hasher* h)
{ return h != NULL ? h->InputBytes : 0; }
XRT_API const xrtblake3error* xrtBlake3HasherError(const xrtblake3hasher* h)
{ return h != NULL ? &h->Error : NULL; }

XRT_API xrtblake3hasher* xrtBlake3HasherClone(const xrtblake3hasher* h)
{
    xrtblake3hasher* copy;
    xrtClearError();
    if (h == NULL) {
        blake3_fail(NULL, XRT_BLAKE3_ERROR_STATE, 0, "invalid blake3 handle");
        return NULL;
    }
    copy = xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, h, sizeof(*copy));
    return copy;
}

XRT_API void xrtBlake3HasherDestroy(xrtblake3hasher* h)
{
    if (h == NULL) return;
    xrtSecureZero(h, sizeof(*h));
    xrtFree(h);
}

/* 三种算法模式仅在初始化时分叉，输出长度由调用方明确指定。 */
static bool blake3_hash(int mode, const void* data, size_t size, const uint8* key,
    const void* context, size_t context_len, uint8* output, size_t outlen,
    xrtblake3error* error)
{
    blake3_hasher h;
    xrtClearError();
    xrtBlake3ErrorInit(error);
    if (output == NULL || outlen == 0 || (size != 0 && data == NULL) ||
        (mode == 1 && key == NULL) || (mode == 2 && context == NULL))
        return blake3_fail(error, XRT_BLAKE3_ERROR_ARGUMENT, 0, "invalid blake3 input, key, context or output");
    if (mode == 1) blake3_hasher_init_keyed(&h, key);
    else if (mode == 2) blake3_hasher_init_derive_key_raw(&h, context, context_len);
    else blake3_hasher_init(&h);
    if (size != 0) blake3_hasher_update(&h, data, size);
    blake3_hasher_finalize(&h, output, outlen);
    xrtSecureZero(&h, sizeof(h));
    return true;
}

XRT_API bool xrtBlake3Hash(const void* data, size_t size, uint8* output,
    size_t outlen, xrtblake3error* error)
{ return blake3_hash(0, data, size, NULL, NULL, 0, output, outlen, error); }
XRT_API bool xrtBlake3HashKeyed(const void* data, size_t size,
    const uint8 key[XRT_BLAKE3_KEY_LEN], uint8* output, size_t outlen, xrtblake3error* error)
{ return blake3_hash(1, data, size, key, NULL, 0, output, outlen, error); }
XRT_API bool xrtBlake3HashKdf(const void* data, size_t size, const void* context,
    size_t context_len, uint8* output, size_t outlen, xrtblake3error* error)
{ return blake3_hash(2, data, size, NULL, context, context_len, output, outlen, error); }

/* reader 从当前游标借用；取消和失败不修改输出，回调原错误优先。 */
XRT_API bool xrtBlake3File(xreader* reader, const xrtresourcelimits* limits,
    uint8* output, size_t outlen, xrtprogressproc progress, ptr user, xrtblake3error* error)
{
    blake3_hasher h;
    xrtresourcelimits defaults;
    uint8 input[XRT_BLAKE3_CHUNK];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false, ok = false;
    xrtClearError();
    xrtBlake3ErrorInit(error);
    if (limits == NULL) { xrtBlake3LimitsInit(&defaults); limits = &defaults; }
    if (!blake3_limits(limits) || reader == NULL || output == NULL || outlen == 0)
        return blake3_fail(error, XRT_BLAKE3_ERROR_ARGUMENT, 0, "invalid blake3 reader, output or limits");
    if (limits->iMaxOutputBytes != 0 && outlen > limits->iMaxOutputBytes)
        return blake3_fail(error, XRT_BLAKE3_ERROR_LIMIT, 0, "blake3 output exceeds maxOutput limit");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    blake3_hasher_init(&h);
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            /* Reader 的 MemoryError/IOError 等原始诊断不能被通用读取错误覆盖。 */
            if (xrtGetError() == NULL) {
                blake3_fail(error, XRT_BLAKE3_ERROR_IO, total, "cannot read blake3 input stream");
            } else if (error != NULL) {
                error->iCode = xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRT_BLAKE3_ERROR_MEMORY : XRT_BLAKE3_ERROR_IO;
                error->iInputOffset = total;
                snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
            }
            goto done;
        }
        if (!blake3_input(total, count, limits->iMaxInputBytes, error)) goto done;
        if (count != 0) blake3_hasher_update(&h, input, count);
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            /* 用户异常保留类别及内容；没有用户错误时才生成取消错误。 */
            if (xrtGetError() == NULL)
                blake3_fail(error, XRT_BLAKE3_ERROR_CANCELLED, total, "blake3 operation cancelled");
            else if (error != NULL) {
                error->iCode = XRT_BLAKE3_ERROR_CANCELLED;
                error->iInputOffset = total;
                snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
            }
            goto done;
        }
        if (count == 0) break;
    }
    blake3_hasher_finalize(&h, output, outlen);
    ok = true;
done:
    xrtSecureZero(input, sizeof(input));
    xrtSecureZero(&h, sizeof(h));
    return ok;
}
