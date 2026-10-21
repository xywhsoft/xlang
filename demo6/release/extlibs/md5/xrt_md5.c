#include "include/xrt_md5.h"
#include <stdio.h>
#include <string.h>

#define XRT_MD5_CHUNK 65536u

// ----------------------------- XRT 摘要状态与错误 -----------------------------

struct xrtmd5hasher {
    xmd5 State;
    xrtmd5options Options;
    xrtmd5error Error;
    bool Failed;
};

static bool md5_publish(const xrtmd5error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_MD5_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_MD5_ERROR_IO: kind = XERR_IO; break;
    case XRT_MD5_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_MD5_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_MD5_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.md5", error->iCode, error->sMessage);
    return false;
}

static bool md5_fail(xrtmd5error* error, int code, uint64 offset, const char* message)
{
    xrtmd5error local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return md5_publish(error);
}

/* 复制错误详情，不替换 Reader、算法或回调已经提供的 XRT 错误。 */
static void md5_observe(xrtmd5error* error, int code, uint64 offset)
{
    if (error == NULL) return;
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
}

static bool md5_limits(const xrtresourcelimits* limits)
{ return limits->iSize == sizeof(*limits) && limits->iVersion == XRT_RESOURCE_LIMITS_VERSION; }

static bool md5_input(uint64 current, uint64 add, uint64 maximum, xrtmd5error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return md5_fail(error, XRT_MD5_ERROR_LIMIT, current, "md5 input exceeds maxInput limit");
    return true;
}

XRT_API void xrtMd5ErrorInit(xrtmd5error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtMd5OptionsInit(xrtmd5options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_MD5_OPTIONS_VERSION;
    xrtResourceLimitsInit(&options->tLimits);
}

// ----------------------------- 有资源预算的流式对象 -----------------------------

XRT_API xrtmd5hasher* xrtMd5HasherCreate(const xrtmd5options* options)
{
    xrtmd5hasher* h;
    xrtClearError();
    if (options != NULL && (options->iSize != sizeof(*options) ||
        options->iVersion != XRT_MD5_OPTIONS_VERSION || !md5_limits(&options->tLimits))) {
        md5_fail(NULL, XRT_MD5_ERROR_ARGUMENT, 0, "invalid md5 options");
        return NULL;
    }
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtMd5OptionsInit(&h->Options);
    xrtMd5Init(&h->State);
    return h;
}

static bool md5_ready(const xrtmd5hasher* h)
{
    if (h == NULL) return md5_fail(NULL, XRT_MD5_ERROR_STATE, 0, "invalid md5 handle");
    return !h->Failed || md5_publish(&h->Error);
}

XRT_API bool xrtMd5HasherUpdate(xrtmd5hasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!md5_ready(h)) return false;
    if (data == NULL && size != 0) {
        h->Failed = true;
        return md5_fail(&h->Error, XRT_MD5_ERROR_ARGUMENT, h->State.Size, "invalid md5 input");
    }
    if (!md5_input(h->State.Size, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    if (!xrtMd5Update(&h->State, data, size)) {
        md5_observe(&h->Error, XRT_MD5_ERROR_LIMIT, h->State.Size);
        h->Failed = true;
        return false;
    }
    return true;
}

XRT_API bool xrtMd5HasherDigest(const xrtmd5hasher* h, uint8 digest[XRT_MD5_DIGEST_SIZE])
{
    xrtClearError();
    if (!md5_ready(h)) return false;
    if (digest == NULL) return md5_fail(NULL, XRT_MD5_ERROR_ARGUMENT, h->State.Size, "invalid md5 output");
    return xrtMd5Final(&h->State, digest);
}

XRT_API bool xrtMd5HasherReset(xrtmd5hasher* h)
{
    xrtClearError();
    if (h == NULL) return md5_fail(NULL, XRT_MD5_ERROR_STATE, 0, "invalid md5 handle");
    xrtMd5Init(&h->State);
    h->Failed = false;
    xrtMd5ErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtMd5HasherInputBytes(const xrtmd5hasher* h)
{ return h != NULL ? h->State.Size : 0; }
XRT_API const xrtmd5error* xrtMd5HasherError(const xrtmd5hasher* h)
{ return h != NULL ? &h->Error : NULL; }
XRT_API void xrtMd5HasherDestroy(xrtmd5hasher* h)
{
    if (h == NULL) return;
    xrtSecureZero(h, sizeof(*h));
    xrtFree(h);
}

// ----------------------------- 一次性与借用 Reader -----------------------------

XRT_API bool xrtMd5Hash(const void* data, size_t size, uint8 digest[XRT_MD5_DIGEST_SIZE],
    xrtmd5error* error)
{
    xrtClearError();
    xrtMd5ErrorInit(error);
    if (digest == NULL || (data == NULL && size != 0))
        return md5_fail(error, XRT_MD5_ERROR_ARGUMENT, 0, "invalid md5 input or output");
    if (xrtMd5(data, size, digest)) return true;
    md5_observe(error, XRT_MD5_ERROR_LIMIT, 0);
    return false;
}

XRT_API bool xrtMd5File(xreader* reader, const xrtresourcelimits* limits,
    uint8 digest[XRT_MD5_DIGEST_SIZE], xrtprogressproc progress, ptr user, xrtmd5error* error)
{
    xrtresourcelimits defaults;
    uint8 input[XRT_MD5_CHUNK];
    xmd5 state;
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false;
    xrtClearError();
    xrtMd5ErrorInit(error);
    if (limits == NULL) { xrtResourceLimitsInit(&defaults); limits = &defaults; }
    if (!md5_limits(limits) || reader == NULL || digest == NULL)
        return md5_fail(error, XRT_MD5_ERROR_ARGUMENT, 0, "invalid md5 reader, output or limits");
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    if (known && !md5_input(0, expected, limits->iMaxInputBytes, error)) return false;
    xrtMd5Init(&state);
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            if (xrtGetError() == NULL)
                return md5_fail(error, XRT_MD5_ERROR_IO, total, "cannot read md5 input stream");
            md5_observe(error, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
                XRT_MD5_ERROR_MEMORY : XRT_MD5_ERROR_IO, total);
            return false;
        }
        if (!md5_input(total, count, limits->iMaxInputBytes, error)) return false;
        if (!xrtMd5Update(&state, input, count)) {
            md5_observe(error, XRT_MD5_ERROR_LIMIT, total);
            return false;
        }
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            if (xrtGetError() == NULL)
                return md5_fail(error, XRT_MD5_ERROR_CANCELLED, total, "md5 operation cancelled");
            md5_observe(error, XRT_MD5_ERROR_CANCELLED, total);
            return false;
        }
        if (count == 0) break;
    }
    /* 末尾回调也可能取消，摘要只在全部操作成功后提交。 */
    return xrtMd5Final(&state, digest);
}
