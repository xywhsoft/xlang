#include "include/xrt_blake2.h"
#include "third_party/blake2.h"
#include <stdio.h>
#include <string.h>

#define XRT_BLAKE2_CHUNK 65536u

/* 参数错误必须在接触密钥和输出缓冲之前确定。 */
static bool blake2_fail(xrtblake2error* error, int code, uint64 input, const char* message)
{
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->iCode = code;
        error->iInputOffset = input;
        snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    }
    return false;
}

/* Reader 的内存/IO 失败保留原始分类和消息。 */
static void blake2_reader_fail(xrtblake2error* error, uint64 input)
{
    const xerror* cause = xrtGetError();
    blake2_fail(error,
        cause != NULL && xrtErrorKind(cause) == XERR_MEMORY ?
            XRT_BLAKE2_ERROR_MEMORY : XRT_BLAKE2_ERROR_IO,
        input, cause != NULL ? xrtErrorMessage(cause) :
            "cannot read blake2 input stream");
}

static bool blake2_parameters(size_t outlen, size_t maximum, const void* key,
    size_t keylen, xrtblake2error* error)
{
    if (outlen == 0 || outlen > maximum)
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 digest size");
    if (keylen > maximum || (keylen != 0 && key == NULL))
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 key");
    return true;
}

static bool blake2_limits(const xrtresourcelimits* limits)
{
    return limits->iSize == sizeof(*limits) &&
        limits->iVersion == XRT_RESOURCE_LIMITS_VERSION &&
        limits->iMaxOutputBytes == 0 && limits->iMaxItemBytes == 0 &&
        limits->iMaxEntries == 0 && limits->iMaxNodes == 0 &&
        limits->iMaxDepth == 0 && limits->iMaxCompressionRatio == 0 &&
        limits->iFlags == 0 && limits->iReserved == 0;
}

static bool blake2_options(const xrtblake2options* options)
{
    return options == NULL || (options->iSize == sizeof(*options) &&
        options->iVersion == XRT_BLAKE2_OPTIONS_VERSION && blake2_limits(&options->tLimits));
}

/* 计数先检查再相加；关闭业务限额也不能发生 uint64 回绕。 */
static bool blake2_input(uint64 current, uint64 add, uint64 maximum, xrtblake2error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return blake2_fail(error, XRT_BLAKE2_ERROR_LIMIT, current, "blake2 input exceeds maxInput limit");
    return true;
}

XRT_API void xrtBlake2ErrorInit(xrtblake2error* error)
{
    if (error != NULL) memset(error, 0, sizeof(*error));
}

XRT_API void xrtBlake2LimitsInit(xrtresourcelimits* limits)
{
    if (limits == NULL) return;
    xrtResourceLimitsInit(limits);
    limits->iMaxOutputBytes = 0;
    limits->iMaxItemBytes = 0;
    limits->iMaxEntries = 0;
    limits->iMaxNodes = 0;
    limits->iMaxDepth = 0;
    limits->iMaxCompressionRatio = 0;
}

XRT_API void xrtBlake2OptionsInit(xrtblake2options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_BLAKE2_OPTIONS_VERSION;
    xrtBlake2LimitsInit(&options->tLimits);
}

/* BLAKE2b：对象独占状态，digest 使用快照，clone 才分配副本。 */
struct xrtblake2bhasher {
    blake2b_state State;
    xrtblake2options Options;
    xrtblake2error Error;
    uint64 InputBytes;
    size_t OutLen;
    uint8 Key[64];
    size_t KeyLen;
    bool Failed;
};

XRT_API xrtblake2bhasher* xrtBlake2bHasherCreate(const xrtblake2options* options,
    size_t outlen, const void* key, size_t keylen)
{
    xrtblake2error error;
    xrtblake2bhasher* h;
    xrtClearError();
    if (!blake2_options(options)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.blake2", 0, "invalid blake2 options");
        return NULL;
    }
    if (!blake2_parameters(outlen, 64, key, keylen, &error)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.blake2", 0, error.sMessage);
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtBlake2OptionsInit(&h->Options);
    h->OutLen = outlen;
    h->KeyLen = keylen;
    if (keylen != 0) {
        memcpy(h->Key, key, keylen);
        blake2b_init_key(&h->State, outlen, key, keylen);
    } else blake2b_init(&h->State, outlen);
    return h;
}

XRT_API bool xrtBlake2bHasherUpdate(xrtblake2bhasher* h, const void* data, size_t size)
{
    if (h == NULL || h->Failed) return false;
    xrtBlake2ErrorInit(&h->Error);
    if (size != 0 && data == NULL) {
        h->Failed = true;
        return blake2_fail(&h->Error, XRT_BLAKE2_ERROR_ARGUMENT, h->InputBytes, "invalid blake2 input");
    }
    if (!blake2_input(h->InputBytes, (uint64)size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    if (size != 0) blake2b_update(&h->State, data, size);
    h->InputBytes += (uint64)size;
    return true;
}

XRT_API bool xrtBlake2bHasherDigest(xrtblake2bhasher* h, uint8* output, size_t outlen)
{
    blake2b_state copy;
    if (h == NULL || h->Failed) return false;
    xrtBlake2ErrorInit(&h->Error);
    if (output == NULL || outlen != h->OutLen)
        return blake2_fail(&h->Error, XRT_BLAKE2_ERROR_ARGUMENT, h->InputBytes,
            "digest size must equal the configured blake2 size");
    copy = h->State;
    blake2b_final(&copy, output, outlen);
    xrtSecureZero(&copy, sizeof(copy));
    return true;
}

XRT_API bool xrtBlake2bHasherReset(xrtblake2bhasher* h)
{
    if (h == NULL) return false;
    xrtSecureZero(&h->State, sizeof(h->State));
    if (h->KeyLen != 0) blake2b_init_key(&h->State, h->OutLen, h->Key, h->KeyLen);
    else blake2b_init(&h->State, h->OutLen);
    h->Failed = false;
    h->InputBytes = 0;
    xrtBlake2ErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtBlake2bHasherInputBytes(const xrtblake2bhasher* h)
{
    return h != NULL ? h->InputBytes : 0;
}

XRT_API size_t xrtBlake2bHasherDigestSize(const xrtblake2bhasher* h)
{
    return h != NULL ? h->OutLen : 0;
}

XRT_API xrtblake2bhasher* xrtBlake2bHasherClone(const xrtblake2bhasher* h)
{
    xrtblake2bhasher* copy;
    if (h == NULL || h->Failed) {
        xrtSetErrorInfo(XERR_STATE, "xrt.blake2", 0, "cannot clone invalid blake2 state");
        return NULL;
    }
    copy = xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, h, sizeof(*copy));
    return copy;
}

XRT_API const xrtblake2error* xrtBlake2bHasherError(const xrtblake2bhasher* h)
{
    return h != NULL ? &h->Error : NULL;
}

XRT_API void xrtBlake2bHasherDestroy(xrtblake2bhasher* h)
{
    if (h == NULL) return;
    xrtSecureZero(h, sizeof(*h));
    xrtFree(h);
}

XRT_API bool xrtBlake2bHash(const void* data, size_t size, size_t outlen,
    const void* key, size_t keylen, uint8* output, xrtblake2error* error)
{
    blake2b_state state;
    xrtBlake2ErrorInit(error);
    if (!blake2_parameters(outlen, 64, key, keylen, error)) return false;
    if (output == NULL || (size != 0 && data == NULL))
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 input or output");
    if (keylen != 0) blake2b_init_key(&state, outlen, key, keylen);
    else blake2b_init(&state, outlen);
    if (size != 0) blake2b_update(&state, data, size);
    blake2b_final(&state, output, outlen);
    xrtSecureZero(&state, sizeof(state));
    return true;
}

/* 从 reader 当前游标开始；取消或失败时不发布部分摘要。 */
XRT_API bool xrtBlake2bFile(xreader* reader, const xrtresourcelimits* limits,
    size_t outlen, uint8* output, xrtprogressproc progress, ptr user, xrtblake2error* error)
{
    blake2b_state state;
    xrtresourcelimits defaults;
    uint8 input[XRT_BLAKE2_CHUNK], digest[64];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false, ok = false;
    xrtBlake2ErrorInit(error);
    if (limits == NULL) { xrtBlake2LimitsInit(&defaults); limits = &defaults; }
    if (!blake2_limits(limits) || reader == NULL || output == NULL ||
        !blake2_parameters(outlen, 64, NULL, 0, error))
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 reader or options");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    blake2b_init(&state, outlen);
    for (;;) {
        size_t count = 0;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            blake2_reader_fail(error, total);
            goto done;
        }
        if (count == 0) break;
        if (!blake2_input(total, count, limits->iMaxInputBytes, error)) goto done;
        blake2b_update(&state, input, (size_t)count);
        total += count;
        if (!xrtProgressReport(progress, user, total, expected, 0,
            known ? XRT_PROGRESS_TOTAL_KNOWN : 0)) {
            blake2_fail(error, XRT_BLAKE2_ERROR_CANCELLED, total, "blake2 operation cancelled");
            goto done;
        }
    }
    blake2b_final(&state, digest, outlen);
    if (!xrtProgressReport(progress, user, total, expected, 0,
        XRT_PROGRESS_FINAL | (known ? XRT_PROGRESS_TOTAL_KNOWN : 0))) {
        blake2_fail(error, XRT_BLAKE2_ERROR_CANCELLED, total, "blake2 operation cancelled");
        goto done;
    }
    memcpy(output, digest, outlen);
    ok = true;
done:
    xrtSecureZero(&state, sizeof(state));
    xrtSecureZero(input, sizeof(input));
    xrtSecureZero(digest, sizeof(digest));
    return ok;
}

/* BLAKE2s：对象独占状态，digest 使用快照，clone 才分配副本。 */
struct xrtblake2shasher {
    blake2s_state State;
    xrtblake2options Options;
    xrtblake2error Error;
    uint64 InputBytes;
    size_t OutLen;
    uint8 Key[32];
    size_t KeyLen;
    bool Failed;
};

XRT_API xrtblake2shasher* xrtBlake2sHasherCreate(const xrtblake2options* options,
    size_t outlen, const void* key, size_t keylen)
{
    xrtblake2error error;
    xrtblake2shasher* h;
    xrtClearError();
    if (!blake2_options(options)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.blake2", 0, "invalid blake2 options");
        return NULL;
    }
    if (!blake2_parameters(outlen, 32, key, keylen, &error)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.blake2", 0, error.sMessage);
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtBlake2OptionsInit(&h->Options);
    h->OutLen = outlen;
    h->KeyLen = keylen;
    if (keylen != 0) {
        memcpy(h->Key, key, keylen);
        blake2s_init_key(&h->State, outlen, key, keylen);
    } else blake2s_init(&h->State, outlen);
    return h;
}

XRT_API bool xrtBlake2sHasherUpdate(xrtblake2shasher* h, const void* data, size_t size)
{
    if (h == NULL || h->Failed) return false;
    xrtBlake2ErrorInit(&h->Error);
    if (size != 0 && data == NULL) {
        h->Failed = true;
        return blake2_fail(&h->Error, XRT_BLAKE2_ERROR_ARGUMENT, h->InputBytes, "invalid blake2 input");
    }
    if (!blake2_input(h->InputBytes, (uint64)size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    if (size != 0) blake2s_update(&h->State, data, size);
    h->InputBytes += (uint64)size;
    return true;
}

XRT_API bool xrtBlake2sHasherDigest(xrtblake2shasher* h, uint8* output, size_t outlen)
{
    blake2s_state copy;
    if (h == NULL || h->Failed) return false;
    xrtBlake2ErrorInit(&h->Error);
    if (output == NULL || outlen != h->OutLen)
        return blake2_fail(&h->Error, XRT_BLAKE2_ERROR_ARGUMENT, h->InputBytes,
            "digest size must equal the configured blake2 size");
    copy = h->State;
    blake2s_final(&copy, output, outlen);
    xrtSecureZero(&copy, sizeof(copy));
    return true;
}

XRT_API bool xrtBlake2sHasherReset(xrtblake2shasher* h)
{
    if (h == NULL) return false;
    xrtSecureZero(&h->State, sizeof(h->State));
    if (h->KeyLen != 0) blake2s_init_key(&h->State, h->OutLen, h->Key, h->KeyLen);
    else blake2s_init(&h->State, h->OutLen);
    h->Failed = false;
    h->InputBytes = 0;
    xrtBlake2ErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtBlake2sHasherInputBytes(const xrtblake2shasher* h)
{
    return h != NULL ? h->InputBytes : 0;
}

XRT_API size_t xrtBlake2sHasherDigestSize(const xrtblake2shasher* h)
{
    return h != NULL ? h->OutLen : 0;
}

XRT_API xrtblake2shasher* xrtBlake2sHasherClone(const xrtblake2shasher* h)
{
    xrtblake2shasher* copy;
    if (h == NULL || h->Failed) {
        xrtSetErrorInfo(XERR_STATE, "xrt.blake2", 0, "cannot clone invalid blake2 state");
        return NULL;
    }
    copy = xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, h, sizeof(*copy));
    return copy;
}

XRT_API const xrtblake2error* xrtBlake2sHasherError(const xrtblake2shasher* h)
{
    return h != NULL ? &h->Error : NULL;
}

XRT_API void xrtBlake2sHasherDestroy(xrtblake2shasher* h)
{
    if (h == NULL) return;
    xrtSecureZero(h, sizeof(*h));
    xrtFree(h);
}

XRT_API bool xrtBlake2sHash(const void* data, size_t size, size_t outlen,
    const void* key, size_t keylen, uint8* output, xrtblake2error* error)
{
    blake2s_state state;
    xrtBlake2ErrorInit(error);
    if (!blake2_parameters(outlen, 32, key, keylen, error)) return false;
    if (output == NULL || (size != 0 && data == NULL))
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 input or output");
    if (keylen != 0) blake2s_init_key(&state, outlen, key, keylen);
    else blake2s_init(&state, outlen);
    if (size != 0) blake2s_update(&state, data, size);
    blake2s_final(&state, output, outlen);
    xrtSecureZero(&state, sizeof(state));
    return true;
}

/* 从 reader 当前游标开始；取消或失败时不发布部分摘要。 */
XRT_API bool xrtBlake2sFile(xreader* reader, const xrtresourcelimits* limits,
    size_t outlen, uint8* output, xrtprogressproc progress, ptr user, xrtblake2error* error)
{
    blake2s_state state;
    xrtresourcelimits defaults;
    uint8 input[XRT_BLAKE2_CHUNK], digest[32];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false, ok = false;
    xrtBlake2ErrorInit(error);
    if (limits == NULL) { xrtBlake2LimitsInit(&defaults); limits = &defaults; }
    if (!blake2_limits(limits) || reader == NULL || output == NULL ||
        !blake2_parameters(outlen, 32, NULL, 0, error))
        return blake2_fail(error, XRT_BLAKE2_ERROR_ARGUMENT, 0, "invalid blake2 reader or options");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    blake2s_init(&state, outlen);
    for (;;) {
        size_t count = 0;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            blake2_reader_fail(error, total);
            goto done;
        }
        if (count == 0) break;
        if (!blake2_input(total, count, limits->iMaxInputBytes, error)) goto done;
        blake2s_update(&state, input, (size_t)count);
        total += count;
        if (!xrtProgressReport(progress, user, total, expected, 0,
            known ? XRT_PROGRESS_TOTAL_KNOWN : 0)) {
            blake2_fail(error, XRT_BLAKE2_ERROR_CANCELLED, total, "blake2 operation cancelled");
            goto done;
        }
    }
    blake2s_final(&state, digest, outlen);
    if (!xrtProgressReport(progress, user, total, expected, 0,
        XRT_PROGRESS_FINAL | (known ? XRT_PROGRESS_TOTAL_KNOWN : 0))) {
        blake2_fail(error, XRT_BLAKE2_ERROR_CANCELLED, total, "blake2 operation cancelled");
        goto done;
    }
    memcpy(output, digest, outlen);
    ok = true;
done:
    xrtSecureZero(&state, sizeof(state));
    xrtSecureZero(input, sizeof(input));
    xrtSecureZero(digest, sizeof(digest));
    return ok;
}
