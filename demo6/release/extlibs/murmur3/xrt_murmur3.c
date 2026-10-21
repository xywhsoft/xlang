#include "include/xrt_murmur3.h"
#include "third_party/MurmurHash3.h"
#include <stdio.h>
#include <string.h>

#define XRT_MURMUR3_CHUNK 65536u

// ----------------------------- 状态与统一错误 -----------------------------

struct xrtmurmur3hasher {
    murmur3_128_state State;
    xrtmurmur3options Options;
    xrtmurmur3error Error;
    bool Failed;
};

static bool murmur3_publish(const xrtmurmur3error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_MURMUR3_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_MURMUR3_ERROR_IO: kind = XERR_IO; break;
    case XRT_MURMUR3_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_MURMUR3_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_MURMUR3_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.murmur3", error->iCode, error->sMessage);
    return false;
}

static bool murmur3_fail(xrtmurmur3error* error, int code, uint64 offset, const char* message)
{
    xrtmurmur3error local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return murmur3_publish(error);
}

/* Reader 和回调的原始错误留在 XRT 中，详情对象只复制描述。 */
static void murmur3_observe(xrtmurmur3error* error, int code, uint64 offset)
{
    if (error == NULL) return;
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
}

static bool murmur3_limits(const xrtresourcelimits* limits)
{ return limits->iSize == sizeof(*limits) && limits->iVersion == XRT_RESOURCE_LIMITS_VERSION; }

static bool murmur3_input(uint64 current, uint64 add, uint64 maximum, xrtmurmur3error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return murmur3_fail(error, XRT_MURMUR3_ERROR_LIMIT, current, "murmur3 input exceeds maxInput limit");
    return true;
}

XRT_API void xrtMurmur3ErrorInit(xrtmurmur3error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtMurmur3OptionsInit(xrtmurmur3options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_MURMUR3_OPTIONS_VERSION;
    xrtResourceLimitsInit(&options->tLimits);
}

// ----------------------------- 有预算的流式对象 -----------------------------

XRT_API xrtmurmur3hasher* xrtMurmur3HasherCreate(const xrtmurmur3options* options, uint32 seed)
{
    xrtmurmur3hasher* h;
    xrtClearError();
    if (options != NULL && (options->iSize != sizeof(*options) ||
        options->iVersion != XRT_MURMUR3_OPTIONS_VERSION || !murmur3_limits(&options->tLimits))) {
        murmur3_fail(NULL, XRT_MURMUR3_ERROR_ARGUMENT, 0, "invalid murmur3 options");
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtMurmur3OptionsInit(&h->Options);
    murmur3_128_init(&h->State, seed);
    return h;
}

static bool murmur3_ready(const xrtmurmur3hasher* h)
{
    if (h == NULL) return murmur3_fail(NULL, XRT_MURMUR3_ERROR_STATE, 0, "invalid murmur3 handle");
    return !h->Failed || murmur3_publish(&h->Error);
}

XRT_API bool xrtMurmur3HasherUpdate(xrtmurmur3hasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!murmur3_ready(h)) return false;
    if (data == NULL && size != 0) {
        h->Failed = true;
        return murmur3_fail(&h->Error, XRT_MURMUR3_ERROR_ARGUMENT, h->State.total, "invalid murmur3 input");
    }
    if (!murmur3_input(h->State.total, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    murmur3_128_update(&h->State, data, size);
    return true;
}

XRT_API bool xrtMurmur3HasherDigest(const xrtmurmur3hasher* h, uint8* digest)
{
    xrtClearError();
    if (!murmur3_ready(h)) return false;
    if (digest == NULL) return murmur3_fail(NULL, XRT_MURMUR3_ERROR_ARGUMENT, h->State.total, "invalid murmur3 output");
    murmur3_128_final(&h->State, digest);
    return true;
}

XRT_API bool xrtMurmur3HasherReset(xrtmurmur3hasher* h)
{
    xrtClearError();
    if (h == NULL) return murmur3_fail(NULL, XRT_MURMUR3_ERROR_STATE, 0, "invalid murmur3 handle");
    murmur3_128_init(&h->State, h->State.seed);
    h->Failed = false;
    xrtMurmur3ErrorInit(&h->Error);
    return true;
}

XRT_API xrtmurmur3hasher* xrtMurmur3HasherClone(const xrtmurmur3hasher* h)
{
    xrtmurmur3hasher* copy;
    xrtClearError();
    if (!murmur3_ready(h)) return NULL;
    copy = xrtMalloc(sizeof(*copy));
    if (copy != NULL) memcpy(copy, h, sizeof(*copy));
    return copy;
}

XRT_API uint64 xrtMurmur3HasherInputBytes(const xrtmurmur3hasher* h)
{ return h != NULL ? h->State.total : 0; }
XRT_API const xrtmurmur3error* xrtMurmur3HasherError(const xrtmurmur3hasher* h)
{ return h != NULL ? &h->Error : NULL; }
XRT_API void xrtMurmur3HasherDestroy(xrtmurmur3hasher* h)
{ xrtFree(h); }

// ----------------------------- 一次性与借用 Reader -----------------------------

XRT_API uint32 xrtMurmur3Hash32(const void* data, size_t size, uint32 seed, xrtmurmur3error* error)
{
    xrtClearError();
    xrtMurmur3ErrorInit(error);
    if (data == NULL && size != 0) {
        murmur3_fail(error, XRT_MURMUR3_ERROR_ARGUMENT, 0, "invalid murmur3 input");
        return 0;
    }
    return murmur3_32(data, size, seed);
}

XRT_API bool xrtMurmur3Hash128(const void* data, size_t size, uint32 seed, uint8* digest, xrtmurmur3error* error)
{
    xrtClearError();
    xrtMurmur3ErrorInit(error);
    if (digest == NULL || (data == NULL && size != 0))
        return murmur3_fail(error, XRT_MURMUR3_ERROR_ARGUMENT, 0, "invalid murmur3 input or output");
    murmur3_128(data, size, seed, digest);
    return true;
}

/* 两种摘要共享读取和取消规则，成功以后才写调用方输出。 */
static bool murmur3_file(xreader* reader, const xrtresourcelimits* limits, uint32 seed,
    bool wide, void* digest, xrtprogressproc progress, ptr user, xrtmurmur3error* error)
{
    union { murmur3_32_state narrow; murmur3_128_state wide; } state;
    xrtresourcelimits defaults;
    uint8 input[XRT_MURMUR3_CHUNK];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false;
    xrtClearError();
    xrtMurmur3ErrorInit(error);
    if (limits == NULL) { xrtResourceLimitsInit(&defaults); limits = &defaults; }
    if (!murmur3_limits(limits) || reader == NULL || digest == NULL)
        return murmur3_fail(error, XRT_MURMUR3_ERROR_ARGUMENT, 0, "invalid murmur3 reader, output or limits");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    if (known && !murmur3_input(0, expected, limits->iMaxInputBytes, error)) return false;
    if (wide) murmur3_128_init(&state.wide, seed);
    else murmur3_32_init(&state.narrow, seed);
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count)) {
            if (xrtGetError() == NULL)
                return murmur3_fail(error, XRT_MURMUR3_ERROR_IO, total, "cannot read murmur3 stream");
            murmur3_observe(error, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
                XRT_MURMUR3_ERROR_MEMORY : XRT_MURMUR3_ERROR_IO, total);
            return false;
        }
        if (!murmur3_input(total, count, limits->iMaxInputBytes, error)) return false;
        if (wide) murmur3_128_update(&state.wide, input, count);
        else murmur3_32_update(&state.narrow, input, count);
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            if (xrtGetError() == NULL)
                return murmur3_fail(error, XRT_MURMUR3_ERROR_CANCELLED, total, "murmur3 operation cancelled");
            murmur3_observe(error, XRT_MURMUR3_ERROR_CANCELLED, total);
            return false;
        }
        if (count == 0) break;
    }
    if (wide) murmur3_128_final(&state.wide, digest);
    else *(uint32*)digest = murmur3_32_final(&state.narrow);
    return true;
}

XRT_API uint32 xrtMurmur3File32(xreader* reader, const xrtresourcelimits* limits,
    uint32 seed, xrtprogressproc progress, ptr user, xrtmurmur3error* error)
{
    uint32 digest = 0;
    murmur3_file(reader, limits, seed, false, &digest, progress, user, error);
    return digest;
}
XRT_API bool xrtMurmur3File128(xreader* reader, const xrtresourcelimits* limits,
    uint32 seed, uint8* digest, xrtprogressproc progress, ptr user, xrtmurmur3error* error)
{ return murmur3_file(reader, limits, seed, true, digest, progress, user, error); }
