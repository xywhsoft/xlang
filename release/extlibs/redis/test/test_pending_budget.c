#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/redis_native.h"
#include <stdio.h>
#include <string.h>

#define LIMIT (64u * 1024u * 1024u)
static unsigned cases, failures;

/* 空 simple string 是最短合法值：+\r\n，共三个字节。独立计算每个
 * 祖先数组剩余兄弟的最低开销，不沿用解析器的状态或内部 helper。 */
static void prefix_case(unsigned depth, unsigned width, int delta)
{
    char prefix[512];
    size_t size = 0u, pending = width - 1u;
    int length = snprintf(prefix, sizeof(prefix), "*%u\r\n", width);
    if (length <= 0) { ++failures; return; }
    size = (size_t)length;
    for (unsigned level = 1u; level < depth; ++level) {
        memcpy(prefix + size, "*2\r\n", 4u);
        size += 4u;
        ++pending;
    }
    /* 这里的 bulk 长度都在八位十进制区间，帧头固定十一字节。 */
    size_t payload = LIMIT - size - 11u - 2u - 3u * pending;
    payload = delta < 0 ? payload - 1u : payload + (unsigned)delta;
    length = snprintf(prefix + size, sizeof(prefix) - size, "$%zu\r\n", payload);
    if (length != 11) { ++failures; return; }
    size += (size_t)length;
    bool invalid = delta > 0;
    bool complete = xr_is_complete_raw(prefix, size);
    const xerror* error = xrtGetError();
    bool ok = !complete && (invalid ? error && xrtErrorKind(error) == XERR_VALUE : !error);
    xrtClearError();

    /* 完整解码对未收齐和畸形均报 ValueError，但诊断不能混为一谈。 */
    xvalue* decoded = xr_decode_raw(prefix, size);
    error = xrtGetError();
    ok = ok && decoded && xrtValueType(decoded) == XVALUE_NULL && error &&
        xrtErrorKind(error) == XERR_VALUE &&
        strcmp(xrtErrorMessage(error), invalid ? "RESP reply is malformed" : "RESP reply is incomplete") == 0;
    xrtValueRelease(decoded);
    xrtClearError();

    /* 两种分包方式分别验证，一字节追加还覆盖 CR 与 LF 分离的状态。 */
    for (unsigned fragmented = 0u; fragmented != 2u; ++fragmented) {
        void* scanner = xr_frame_create();
        if (!scanner) { ok = false; break; }
        size_t first = fragmented ? 0u : size;
        for (size_t at = first; at <= size; ++at) {
            complete = xr_frame_check(scanner, prefix, at);
            error = xrtGetError();
            bool rejected = invalid && at == size;
            ok = ok && !complete && (rejected ? error && xrtErrorKind(error) == XERR_VALUE : !error);
        }
        xr_frame_destroy(scanner);
        xrtClearError();
    }
    ++cases;
    if (!ok) {
        ++failures;
        if (failures <= 8u) fprintf(stderr, "pending budget depth=%u width=%u delta=%d\n", depth, width, delta);
    }
}

/* 确认预留不会误拒绝合法大帧：一条空 simple string 作为第二个子值，
 * 整条 RESP 恰好占满 64 MiB。只分配一次正文，不为各短前缀制造大样本。 */
static void exact_complete(void)
{
    xbuffer buffer;
    (void)xrtBufferInit(&buffer);
    bool ok = xrtBufferAppend(&buffer, XRT_BYTES_LITERAL("*2\r\n$67108844\r\n")) &&
        xrtBufferReserve(&buffer, LIMIT);
    if (ok) {
        memset(buffer.Data + buffer.Size, 'x', 67108844u);
        buffer.Size += 67108844u;
        ok = xrtBufferAppend(&buffer, XRT_BYTES_LITERAL("\r\n+\r\n"));
    }
    void* scanner = xr_frame_create();
    ok = ok && scanner && buffer.Size == LIMIT && xr_is_complete_raw(buffer.Data, buffer.Size) &&
        xr_frame_check(scanner, buffer.Data, buffer.Size) && !xrtGetError();
    xvalue* decoded = ok ? xr_decode_raw(buffer.Data, buffer.Size) : NULL;
    ok = ok && decoded && xrtValueType(decoded) == XVALUE_OBJECT && !xrtGetError();
    xrtValueRelease(decoded);
    xr_frame_destroy(scanner);
    xrtBufferUnit(&buffer);
    xrtClearError();
    ++cases;
    if (!ok) { ++failures; fprintf(stderr, "exactly full pending budget frame failed\n"); }
}

int main(void)
{
    static const unsigned depths[] = {1u, 2u, 3u, 8u, 32u, 62u, 63u};
    static const unsigned widths[] = {1u, 2u, 3u, 127u};
    for (size_t d = 0u; d < sizeof(depths) / sizeof(depths[0]); ++d)
        for (size_t w = 0u; w < sizeof(widths) / sizeof(widths[0]); ++w)
            for (int delta = -1; delta <= 1; ++delta) prefix_case(depths[d], widths[w], delta);
    exact_complete();
    printf("redis pending-byte budget: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
