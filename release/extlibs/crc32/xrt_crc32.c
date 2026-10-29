#include "include/xrt_crc32.h"
#include <stdio.h>
#include <string.h>

#define XRT_CRC32_CHUNK 65536u

// ----------------------------- CRC-32/ISO-HDLC -----------------------------

/* 表只构造一次，发布后只读；复用 XRT Once，不增加常量资源或私有同步实现。 */
static uint32 crc32_table[8][256];
static xonce crc32_once = XRT_ONCE_INIT;

static bool crc32_init(ptr unused)
{
    uint32 i, j;
    (void)unused;
    for (i = 0; i < 256; ++i) {
        uint32 crc = i;
        for (j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        crc32_table[0][i] = crc;
    }
    for (i = 0; i < 256; ++i) {
        uint32 crc = crc32_table[0][i];
        for (j = 1; j < 8; ++j) {
            crc = crc32_table[0][crc & 255] ^ (crc >> 8);
            crc32_table[j][i] = crc;
        }
    }
    return true;
}

/* 显式小端装载：不依赖宿主字节序、指针对齐或别名规则。 */
static uint32 crc32_le32(const uint8* p)
{ return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24); }

static uint32 crc32_update(uint32 value, const uint8* data, size_t size)
{
    uint32 crc = value ^ 0xFFFFFFFFu;
    while (size >= 8) {
        uint32 lo = crc32_le32(data) ^ crc, hi = crc32_le32(data + 4);
        crc = crc32_table[7][lo & 255] ^ crc32_table[6][(lo >> 8) & 255] ^
            crc32_table[5][(lo >> 16) & 255] ^ crc32_table[4][lo >> 24] ^
            crc32_table[3][hi & 255] ^ crc32_table[2][(hi >> 8) & 255] ^
            crc32_table[1][(hi >> 16) & 255] ^ crc32_table[0][hi >> 24];
        data += 8;
        size -= 8;
    }
    while (size--) crc = crc32_table[0][(crc ^ *data++) & 255] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ----------------------------- 错误与输入合同 -----------------------------

struct xrtcrc32hasher {
    uint32 Crc;
    xrtcrc32options Options;
    xrtcrc32error Error;
    uint64 InputBytes;
    bool Failed;
};

static bool crc32_publish(const xrtcrc32error* error)
{
    xerrkind kind = XERR_ARGUMENT;
    switch (error->iCode) {
    case XRT_CRC32_ERROR_MEMORY: kind = XERR_MEMORY; break;
    case XRT_CRC32_ERROR_IO: kind = XERR_IO; break;
    case XRT_CRC32_ERROR_LIMIT: kind = XERR_RANGE; break;
    case XRT_CRC32_ERROR_STATE: kind = XERR_STATE; break;
    case XRT_CRC32_ERROR_CANCELLED: kind = XERR_CANCELLED; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.crc32", error->iCode, error->sMessage);
    return false;
}

static bool crc32_fail(xrtcrc32error* error, int code, uint64 offset, const char* message)
{
    xrtcrc32error local;
    if (error == NULL) error = &local;
    memset(error, 0, sizeof(*error));
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", message);
    return crc32_publish(error);
}

static bool crc32_limits(const xrtresourcelimits* limits)
{ return limits->iSize == sizeof(*limits) && limits->iVersion == XRT_RESOURCE_LIMITS_VERSION; }

static bool crc32_input(uint64 current, uint64 add, uint64 maximum, xrtcrc32error* error)
{
    if (add > UINT64_MAX - current ||
        (maximum != 0 && (current > maximum || add > maximum - current)))
        return crc32_fail(error, XRT_CRC32_ERROR_LIMIT, current, "crc32 input exceeds maxInput limit");
    return true;
}

XRT_API void xrtCrc32ErrorInit(xrtcrc32error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }
XRT_API void xrtCrc32OptionsInit(xrtcrc32options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_CRC32_OPTIONS_VERSION;
    xrtResourceLimitsInit(&options->tLimits);
}

// ----------------------------- 流式状态 -----------------------------

XRT_API xrtcrc32hasher* xrtCrc32HasherCreate(const xrtcrc32options* options)
{
    xrtcrc32hasher* h;
    xrtClearError();
    if (options != NULL && (options->iSize != sizeof(*options) ||
        options->iVersion != XRT_CRC32_OPTIONS_VERSION || !crc32_limits(&options->tLimits))) {
        crc32_fail(NULL, XRT_CRC32_ERROR_ARGUMENT, 0, "invalid crc32 options");
        return NULL;
    }
    if (!xrtOnce(&crc32_once, crc32_init, NULL)) return NULL;
    h = xrtMalloc(sizeof(*h));
    if (h == NULL) return NULL;
    memset(h, 0, sizeof(*h));
    if (options != NULL) h->Options = *options;
    else xrtCrc32OptionsInit(&h->Options);
    return h;
}

static bool crc32_ready(const xrtcrc32hasher* h)
{
    if (h == NULL) return crc32_fail(NULL, XRT_CRC32_ERROR_STATE, 0, "invalid crc32 handle");
    return !h->Failed || crc32_publish(&h->Error);
}

XRT_API bool xrtCrc32HasherUpdate(xrtcrc32hasher* h, const void* data, size_t size)
{
    xrtClearError();
    if (!crc32_ready(h)) return false;
    if (data == NULL && size != 0) {
        h->Failed = true;
        return crc32_fail(&h->Error, XRT_CRC32_ERROR_ARGUMENT, h->InputBytes, "invalid crc32 input");
    }
    if (!crc32_input(h->InputBytes, size, h->Options.tLimits.iMaxInputBytes, &h->Error)) {
        h->Failed = true;
        return false;
    }
    h->Crc = crc32_update(h->Crc, data, size);
    h->InputBytes += size;
    return true;
}

XRT_API bool xrtCrc32HasherDigest(const xrtcrc32hasher* h, uint32* crc)
{
    xrtClearError();
    if (!crc32_ready(h)) return false;
    if (crc == NULL) return crc32_fail(NULL, XRT_CRC32_ERROR_ARGUMENT, h->InputBytes, "invalid crc32 output");
    *crc = h->Crc;
    return true;
}

XRT_API bool xrtCrc32HasherReset(xrtcrc32hasher* h)
{
    xrtClearError();
    if (h == NULL) return crc32_fail(NULL, XRT_CRC32_ERROR_STATE, 0, "invalid crc32 handle");
    h->Crc = 0;
    h->InputBytes = 0;
    h->Failed = false;
    xrtCrc32ErrorInit(&h->Error);
    return true;
}

XRT_API uint64 xrtCrc32HasherInputBytes(const xrtcrc32hasher* h)
{ return h != NULL ? h->InputBytes : 0; }
XRT_API const xrtcrc32error* xrtCrc32HasherError(const xrtcrc32hasher* h)
{ return h != NULL ? &h->Error : NULL; }
XRT_API void xrtCrc32HasherDestroy(xrtcrc32hasher* h)
{ xrtFree(h); }

XRT_API bool xrtCrc32Hash(const void* data, size_t size, uint32* crc, xrtcrc32error* error)
{
    xrtClearError();
    xrtCrc32ErrorInit(error);
    if (crc == NULL || (data == NULL && size != 0))
        return crc32_fail(error, XRT_CRC32_ERROR_ARGUMENT, 0, "invalid crc32 input or output");
    if (!xrtOnce(&crc32_once, crc32_init, NULL)) return false;
    *crc = crc32_update(0, data, size);
    return true;
}

// ----------------------------- 借用 Reader -----------------------------

/* 只复制诊断；Reader 和回调的 XRT 错误类别、消息与所有权保持原样。 */
static void crc32_observe(xrtcrc32error* error, int code, uint64 offset)
{
    if (error == NULL) return;
    error->iCode = code;
    error->iInputOffset = offset;
    snprintf(error->sMessage, sizeof(error->sMessage), "%s", xrtErrorMessage(xrtGetError()));
}

XRT_API bool xrtCrc32File(xreader* reader, const xrtresourcelimits* limits,
    uint32* crc, xrtprogressproc progress, ptr user, xrtcrc32error* error)
{
    xrtresourcelimits defaults;
    uint8 input[XRT_CRC32_CHUNK];
    uint32 value = 0;
    uint64 total = 0, expected = 0, position = 0, end = 0;
    bool known = false;
    xrtClearError();
    xrtCrc32ErrorInit(error);
    if (limits == NULL) { xrtResourceLimitsInit(&defaults); limits = &defaults; }
    if (!crc32_limits(limits) || reader == NULL || crc == NULL)
        return crc32_fail(error, XRT_CRC32_ERROR_ARGUMENT, 0, "invalid crc32 reader, output or limits");
    if (!xrtOnce(&crc32_once, crc32_init, NULL)) return false;
    if (xrtReaderCanSeek(reader)) {
        known = xrtReaderTell(reader, &position) && xrtReaderSize(reader, &end) && end >= position;
        if (known) expected = end - position;
        else xrtClearError();
    }
    if (known && !crc32_input(0, expected, limits->iMaxInputBytes, error)) return false;
    for (;;) {
        size_t count = 0;
        uint32 flags;
        if (!xrtReaderRead(reader, input, sizeof(input), &count) || count > sizeof(input)) {
            if (xrtGetError() == NULL)
                return crc32_fail(error, XRT_CRC32_ERROR_IO, total, "cannot read crc32 input stream");
            crc32_observe(error, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
                XRT_CRC32_ERROR_MEMORY : XRT_CRC32_ERROR_IO, total);
            return false;
        }
        if (!crc32_input(total, count, limits->iMaxInputBytes, error)) return false;
        value = crc32_update(value, input, count);
        total += count;
        flags = (known ? XRT_PROGRESS_TOTAL_KNOWN : 0) | (count == 0 ? XRT_PROGRESS_FINAL : 0);
        if (!xrtProgressReport(progress, user, total, expected, 0, flags)) {
            if (xrtGetError() == NULL)
                return crc32_fail(error, XRT_CRC32_ERROR_CANCELLED, total, "crc32 operation cancelled");
            crc32_observe(error, XRT_CRC32_ERROR_CANCELLED, total);
            return false;
        }
        if (count == 0) break;
    }
    /* 末尾通知也可能取消，所有步骤成功后才提交输出。 */
    *crc = value;
    return true;
}
