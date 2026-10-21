#include "include/xrt_xxhash.h"
#define XXH_STATIC_LINKING_ONLY
#include "third_party/xxhash.h"
#include <stdio.h>
#include <string.h>

#define XRT_XXHASH_CHUNK 65536u

/* 状态直接内嵌，只经 XRT 分配一次，不另走上游 CRT 分配器。 */
struct xrtxxhashhasher {
    XXH64_state_t State;
    xrtxxhashoptions Options;
    xrtxxhasherror Error;
    uint64 InputBytes;
    bool Failed;
};

static bool xxhash_publish(const xrtxxhasherror* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_XXHASH_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_XXHASH_ERROR_IO: kind = XERR_IO; break;
    case XRT_XXHASH_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_XXHASH_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_XXHASH_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.xxhash", error->iCode, error->sMessage);
    return false;
}

static bool xxhash_fail(xrtxxhasherror* error, int code, uint64 offset, const char* message)
{
    xrtxxhasherror local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return xxhash_publish(error);
}

static bool xxhash_limits(const xrtresourcelimits* limits)
{
    return limits->iSize == sizeof(*limits) &&
        limits->iVersion == XRT_RESOURCE_LIMITS_VERSION &&
        limits->iMaxOutputBytes == 0 && limits->iMaxItemBytes == 0 &&
        limits->iMaxEntries == 0 && limits->iMaxNodes == 0 &&
        limits->iMaxDepth == 0 && limits->iMaxCompressionRatio == 0 &&
        limits->iFlags == 0 && limits->iReserved == 0;
}

XRT_API void xrtXxhashLimitsInit(xrtresourcelimits* limits)
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

static bool xxhash_input(uint64 current, uint64 add, uint64 maximum, xrtxxhasherror* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return xxhash_fail(error, XRT_XXHASH_ERROR_LIMIT, current, "xxhash input exceeds maxInput limit");
    return true;
}

XRT_API void xrtXxhashErrorInit(xrtxxhasherror* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtXxhashOptionsInit(xrtxxhashoptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_XXHASH_OPTIONS_VERSION;
    xrtXxhashLimitsInit(&options->tLimits);
}

// -------------------------------- 流式状态 --------------------------------

XRT_API xrtxxhashhasher* xrtXxhashHasherCreate(const xrtxxhashoptions* options)
{
    xrtxxhashhasher* h;
    xrtClearError();
    if (options != NULL && (options->iSize != sizeof(*options) ||
        options->iVersion != XRT_XXHASH_OPTIONS_VERSION || !xxhash_limits(&options->tLimits))) {
        xxhash_fail(NULL, XRT_XXHASH_ERROR_ARGUMENT, 0, "invalid xxhash options");
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtXxhashOptionsInit(&h->Options);
    XXH64_reset(&h->State, h->Options.iSeed);
    return h;
}

static bool xxhash_ready(const xrtxxhashhasher* h)
{
    if (h == NULL) return xxhash_fail(NULL, XRT_XXHASH_ERROR_STATE, 0, "invalid xxhash handle");
    return !h->Failed || xxhash_publish(&h->Error);
}

XRT_API bool xrtXxhashHasherUpdate(xrtxxhashhasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!xxhash_ready(h)) return false;
    if (data == NULL && size != 0) {
        h->Failed = true;
        return xxhash_fail(&h->Error, XRT_XXHASH_ERROR_ARGUMENT, h->InputBytes, "invalid xxhash input");
    }
    if (!xxhash_input(h->InputBytes, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    if (size != 0) XXH64_update(&h->State, data, size);
    h->InputBytes += size;
    return true;
}

XRT_API bool xrtXxhashHasherDigest(const xrtxxhashhasher* h, uint64* hash)
{
    xrtClearError();
    if (!xxhash_ready(h)) return false;
    if (hash == NULL) return xxhash_fail(NULL, XRT_XXHASH_ERROR_ARGUMENT, h->InputBytes, "invalid xxhash output");
    *hash = XXH64_digest(&h->State);
    return true;
}

XRT_API bool xrtXxhashHasherReset(xrtxxhashhasher* h)
{
    xrtClearError();
    if (h == NULL) return xxhash_fail(NULL, XRT_XXHASH_ERROR_STATE, 0, "invalid xxhash handle");
    XXH64_reset(&h->State, h->Options.iSeed);
    h->InputBytes = 0;
    h->Failed = false;
    xrtXxhashErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtXxhashHasherInputBytes(const xrtxxhashhasher* h)
{
    xrtClearError();
    if (h == NULL) {
        xxhash_fail(NULL, XRT_XXHASH_ERROR_STATE, 0, "invalid xxhash handle");
        return 0;
    }
    return h->InputBytes;
}
XRT_API const xrtxxhasherror* xrtXxhashHasherError(const xrtxxhashhasher* h)
{ return h != NULL ? &h->Error : NULL; }
XRT_API void xrtXxhashHasherDestroy(xrtxxhashhasher* h)
{ xrtFree(h); }

// -------------------------------- 一次性接口 --------------------------------

XRT_API bool xrtXxhash64(const void* data, size_t size, uint64 seed, uint64* hash, xrtxxhasherror* error)
{
    xrtClearError();
    xrtXxhashErrorInit(error);
    if (hash == NULL || (data == NULL && size != 0))
        return xxhash_fail(error, XRT_XXHASH_ERROR_ARGUMENT, 0, "invalid xxhash input or output");
    *hash = XXH64(data, size, seed);
    return true;
}

XRT_API bool xrtXxhash32(const void* data, size_t size, uint32 seed, uint32* hash, xrtxxhasherror* error)
{
    xrtClearError();
    xrtXxhashErrorInit(error);
    if (hash == NULL || (data == NULL && size != 0))
        return xxhash_fail(error, XRT_XXHASH_ERROR_ARGUMENT, 0, "invalid xxhash input or output");
    *hash = XXH32(data, size, seed);
    return true;
}

// -------------------------------- 借用 Reader --------------------------------

/* 复制可选详细诊断，不覆盖 Reader/用户回调产生的原始 XRT 异常。 */
static void xxhash_observe(xrtxxhasherror* error, int code, uint64 offset)
{
    if (error == NULL) return;
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
}

XRT_API bool xrtXxhashFile64(xreader* reader, uint64 seed, const xrtresourcelimits* limits,
    uint64* hash, xrtprogressproc progress, ptr user, xrtxxhasherror* error)
{
    XXH64_state_t state;
    xrtresourcelimits defaults;
    uint8 input[XRT_XXHASH_CHUNK];
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false;
    xrtClearError();
    xrtXxhashErrorInit(error);
    if (limits == NULL) { xrtXxhashLimitsInit(&defaults); limits = &defaults; }
    if (!xxhash_limits(limits) || reader == NULL || hash == NULL)
        return xxhash_fail(error, XRT_XXHASH_ERROR_ARGUMENT, 0, "invalid xxhash reader, output or limits");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    if (known && !xxhash_input(0, expected, limits->iMaxInputBytes, error)) return false;
    XXH64_reset(&state, seed);
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            if (xrtGetError() == NULL)
                return xxhash_fail(error, XRT_XXHASH_ERROR_IO, total, "cannot read xxhash input stream");
            xxhash_observe(error, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
                XRT_XXHASH_ERROR_MEMORY : XRT_XXHASH_ERROR_IO, total);
            return false;
        }
        if (!xxhash_input(total, count, limits->iMaxInputBytes, error)) return false;
        if (count != 0) XXH64_update(&state, input, count);
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            if (xrtGetError() == NULL)
                return xxhash_fail(error, XRT_XXHASH_ERROR_CANCELLED, total, "xxhash operation cancelled");
            xxhash_observe(error, XRT_XXHASH_ERROR_CANCELLED, total);
            return false;
        }
        if (count == 0) break;
    }
    /* 包含末尾回调在内的所有步骤成功后才写输出。 */
    *hash = XXH64_digest(&state);
    return true;
}
