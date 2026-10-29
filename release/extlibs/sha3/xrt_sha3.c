#include "include/xrt_sha3.h"
#include "third_party/sha3.h"
#include <stdio.h>
#include <string.h>

#define XRT_SHA3_CHUNK 65536u

// ----------------------------- 状态与统一错误 -----------------------------

struct xrtsha3hasher {
    sha3_ctx State;
    xrtsha3options Options;
    xrtsha3error Error;
    uint64 InputBytes;
    int Bits;
    bool IsShake;
    bool Failed;
};

static bool sha3_publish(const xrtsha3error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_SHA3_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_SHA3_ERROR_IO: kind = XERR_IO; break;
    case XRT_SHA3_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_SHA3_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_SHA3_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.sha3", error->iCode, error->sMessage);
    return false;
}
static bool sha3_fail(xrtsha3error* error, int code, uint64 offset, const char* message)
{
    xrtsha3error local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return sha3_publish(error);
}

/* 原始 Reader/回调错误由 XRT 保管，详情只记录位置与描述。 */
static void sha3_observe(xrtsha3error* error, int code, uint64 offset)
{
    if (error == NULL) return;
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
}
static bool sha3_bits(int bits, bool shake)
{ return shake ? bits == 128 || bits == 256 : bits == 224 || bits == 256 || bits == 384 || bits == 512; }
static bool sha3_limits(const xrtresourcelimits* limits)
{ return limits->iSize == sizeof(*limits) && limits->iVersion == XRT_RESOURCE_LIMITS_VERSION; }
static bool sha3_input(uint64 current, uint64 add, uint64 maximum, xrtsha3error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return sha3_fail(error, XRT_SHA3_ERROR_LIMIT, current, "sha3 input exceeds maxInput limit");
    return true;
}
static bool sha3_output(int bits, bool shake, size_t size, xrtsha3error* error)
{
    if (!sha3_bits(bits, shake))
        return sha3_fail(error, XRT_SHA3_ERROR_ARGUMENT, 0, "invalid sha3 bits or shake strength");
    if (!shake && size != (size_t)bits / 8)
        return sha3_fail(error, XRT_SHA3_ERROR_ARGUMENT, 0, "sha3 requires its natural output length");
    return true;
}

XRT_API void xrtSha3ErrorInit(xrtsha3error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }
XRT_API void xrtSha3OptionsInit(xrtsha3options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_SHA3_OPTIONS_VERSION;
    xrtResourceLimitsInit(&options->tLimits);
}
XRT_API size_t xrtSha3DigestSize(int bits)
{
    xrtClearError();
    if (!sha3_bits(bits, false)) {
        sha3_fail(NULL, XRT_SHA3_ERROR_ARGUMENT, 0, "invalid sha3 bits");
        return 0;
    }
    return (size_t)bits / 8;
}

// ----------------------------- 流式对象与非终结快照 -----------------------------

static xrtsha3hasher* sha3_create(const xrtsha3options* options, int bits, bool shake)
{
    xrtsha3hasher* h;
    xrtClearError();
    if (!sha3_bits(bits, shake) || (options != NULL &&
        (options->iSize != sizeof(*options) || options->iVersion != XRT_SHA3_OPTIONS_VERSION ||
         !sha3_limits(&options->tLimits)))) {
        sha3_fail(NULL, XRT_SHA3_ERROR_ARGUMENT, 0, "invalid sha3 algorithm or options");
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtSha3OptionsInit(&h->Options);
    h->Bits = bits;
    h->IsShake = shake;
    if (shake) shake_init(&h->State, bits);
    else sha3_init(&h->State, bits);
    return h;
}
XRT_API xrtsha3hasher* xrtSha3HasherCreate(const xrtsha3options* options, int bits)
{ return sha3_create(options, bits, false); }
XRT_API xrtsha3hasher* xrtSha3ShakeHasherCreate(const xrtsha3options* options, int strength)
{ return sha3_create(options, strength, true); }

static bool sha3_ready(const xrtsha3hasher* h)
{
    if (h == NULL) return sha3_fail(NULL, XRT_SHA3_ERROR_STATE, 0, "invalid sha3 handle");
    return !h->Failed || sha3_publish(&h->Error);
}
XRT_API bool xrtSha3HasherUpdate(xrtsha3hasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!sha3_ready(h)) return false;
    if (data == NULL && size != 0) {
        h->Failed = true;
        return sha3_fail(&h->Error, XRT_SHA3_ERROR_ARGUMENT, h->InputBytes, "invalid sha3 input");
    }
    if (!sha3_input(h->InputBytes, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    sha3_update(&h->State, data, size);
    h->InputBytes += size;
    return true;
}
XRT_API bool xrtSha3HasherCheckOutput(const xrtsha3hasher* h, size_t size)
{
    xrtClearError();
    if (!sha3_ready(h) || !sha3_output(h->Bits, h->IsShake, size, NULL)) return false;
    if (h->Options.tLimits.iMaxOutputBytes != 0 && size > h->Options.tLimits.iMaxOutputBytes)
        return sha3_fail(NULL, XRT_SHA3_ERROR_LIMIT, h->InputBytes, "sha3 output exceeds maxOutput limit");
    return true;
}
XRT_API bool xrtSha3HasherDigest(const xrtsha3hasher* h, uint8* output, size_t size)
{
    sha3_ctx copy;
    if (!xrtSha3HasherCheckOutput(h, size)) return false;
    if (output == NULL && size != 0)
        return sha3_fail(NULL, XRT_SHA3_ERROR_ARGUMENT, h->InputBytes, "invalid sha3 output");
    copy = h->State;
    if (h->IsShake) shake_final(&copy, output, size);
    else sha3_final(&copy, output, size);
    return true;
}
XRT_API bool xrtSha3HasherReset(xrtsha3hasher* h)
{
    xrtClearError();
    if (h == NULL) return sha3_fail(NULL, XRT_SHA3_ERROR_STATE, 0, "invalid sha3 handle");
    if (h->IsShake) shake_init(&h->State, h->Bits);
    else sha3_init(&h->State, h->Bits);
    h->InputBytes = 0;
    h->Failed = false;
    xrtSha3ErrorInit(&h->Error);
    return true;
}
XRT_API xrtsha3hasher* xrtSha3HasherClone(const xrtsha3hasher* h)
{
    xrtsha3hasher* copy;
    xrtClearError();
    if (!sha3_ready(h)) return NULL;
    copy = xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, h, sizeof(*copy));
    return copy;
}
XRT_API uint64 xrtSha3HasherInputBytes(const xrtsha3hasher* h)
{ return h != NULL ? h->InputBytes : 0; }
XRT_API int xrtSha3HasherOutBytes(const xrtsha3hasher* h)
{ return h != NULL && !h->IsShake ? h->Bits / 8 : 0; }
XRT_API const xrtsha3error* xrtSha3HasherError(const xrtsha3hasher* h)
{ return h != NULL ? &h->Error : NULL; }
XRT_API void xrtSha3HasherDestroy(xrtsha3hasher* h)
{ xrtFree(h); }

// ----------------------------- 一次性摘要与借用 Reader -----------------------------

static bool sha3_hash_once(const void* data, size_t size, int bits, bool shake,
    uint8* output, size_t length, xrtsha3error* error)
{
    xrtClearError();
    xrtSha3ErrorInit(error);
    if (!sha3_output(bits, shake, length, error)) return false;
    if ((data == NULL && size != 0) || (output == NULL && length != 0))
        return sha3_fail(error, XRT_SHA3_ERROR_ARGUMENT, 0, "invalid sha3 input or output");
    if (shake) shake_hash(data, size, bits, output, length);
    else sha3_hash(data, size, bits, output, length);
    return true;
}
XRT_API bool xrtSha3Hash(const void* data, size_t size, int bits, uint8* output, size_t length, xrtsha3error* error)
{ return sha3_hash_once(data, size, bits, false, output, length, error); }
XRT_API bool xrtShakeHash(const void* data, size_t size, int strength, uint8* output, size_t length, xrtsha3error* error)
{ return sha3_hash_once(data, size, strength, true, output, length, error); }

XRT_API bool xrtSha3File(xreader* reader, const xrtresourcelimits* limits, int bits,
    uint8* output, size_t length, xrtprogressproc progress, ptr user, xrtsha3error* error)
{
    xrtresourcelimits defaults;
    sha3_ctx state;
    uint8 input[XRT_SHA3_CHUNK];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false;
    xrtClearError();
    xrtSha3ErrorInit(error);
    if (limits == NULL) { xrtResourceLimitsInit(&defaults); limits = &defaults; }
    if (reader == NULL || output == NULL || !sha3_limits(limits))
        return sha3_fail(error, XRT_SHA3_ERROR_ARGUMENT, 0, "invalid sha3 reader, output or limits");
    if (!sha3_output(bits, false, length, error)) return false;
    if (limits->iMaxOutputBytes != 0 && length > limits->iMaxOutputBytes)
        return sha3_fail(error, XRT_SHA3_ERROR_LIMIT, 0, "sha3 output exceeds maxOutput limit");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    if (known && !sha3_input(0, expected, limits->iMaxInputBytes, error)) return false;
    sha3_init(&state, bits);
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count)) {
            if (xrtGetError() == NULL) return sha3_fail(error, XRT_SHA3_ERROR_IO, total, "cannot read sha3 stream");
            sha3_observe(error, xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRT_SHA3_ERROR_MEMORY : XRT_SHA3_ERROR_IO, total);
            return false;
        }
        if (!sha3_input(total, count, limits->iMaxInputBytes, error)) return false;
        sha3_update(&state, input, count);
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            if (xrtGetError() == NULL) return sha3_fail(error, XRT_SHA3_ERROR_CANCELLED, total, "sha3 operation cancelled");
            sha3_observe(error, XRT_SHA3_ERROR_CANCELLED, total);
            return false;
        }
        if (count == 0) break;
    }
    /* EOF 回调也可以取消；成功确认后才写入调用方的摘要。 */
    sha3_final(&state, output, length);
    return true;
}
