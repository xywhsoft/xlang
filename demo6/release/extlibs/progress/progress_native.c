/* 进度模型和单行文本渲染；无终端控制副作用。 */
#include "include/progress_native.h"
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#define XPROG_DOMAIN "xlang.extlib.progress"
#define XPROG_PREFIX_MAX 1024u

typedef struct {
    long long total;
    long long current;
    int width;
    char *prefix;
    size_t prefixSize;
} XProg;

static int xprog_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XPROG_DOMAIN, 0, message);
    return 0;
}

void *xprog_create(int64_t total) {
    XProg *progress;
    xrtClearError();
    if (total <= 0) { xprog_error("total must be positive"); return NULL; }
    progress = (XProg *)xrtMalloc(sizeof(*progress));
    if (progress == NULL) return NULL;
    memset(progress, 0, sizeof(*progress));
    progress->total = total;
    progress->width = 40;
    progress->prefix = (char *)xrtMalloc(9);
    if (progress->prefix == NULL) { xrtFree(progress); return NULL; }
    memcpy(progress->prefix, "Progress", 9);
    progress->prefixSize = 8;
    return progress;
}

void xprog_destroy(void *handle) {
    XProg *progress = (XProg *)handle;
    if (progress == NULL) return;
    xrtFree(progress->prefix);
    xrtFree(progress);
}

int xprog_advance(void *handle, int64_t count) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL || count < 0) return xprog_error("advance requires a live bar and non-negative count");
    /* 用剩余量比较，避免 current + count 在大计数时溢出。 */
    if (count >= progress->total - progress->current) progress->current = progress->total;
    else progress->current += count;
    return 1;
}

int xprog_set(void *handle, int64_t current) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL) return xprog_error("progress bar is destroyed");
    progress->current = current < 0 ? 0 :
                        current > progress->total ? progress->total : current;
    return 1;
}

int xprog_set_prefix_n(void *handle, const char *prefix, size_t size) {
    XProg *progress = (XProg *)handle;
    char *copy;
    size_t i;
    xrtClearError();
    if (progress == NULL || prefix == NULL || size > XPROG_PREFIX_MAX)
        return xprog_error("invalid progress prefix");
    /* 不可见格式符和双向控制符会破坏单行进度文本的阅读顺序。 */
    for (i = 0; i < size;) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){prefix + i, size - i},
            &scalar, &read) != XUTF_OK || read == 0u || scalar < 0x20u ||
            (scalar >= 0x7fu && scalar <= 0x9fu) ||
            scalar == 0x00adu || scalar == 0x2028u || scalar == 0x2029u ||
            scalar == 0x061cu || (scalar >= 0x200bu && scalar <= 0x200fu) ||
            (scalar >= 0x202au && scalar <= 0x202eu) ||
            (scalar >= 0x2060u && scalar <= 0x206fu) ||
            scalar == 0xfeffu)
            return xprog_error("progress prefix must be single-line UTF-8 text");
        i += read;
    }
    copy = (char *)xrtMalloc(size + 1);
    if (copy == NULL) return 0;
    memcpy(copy, prefix, size);
    copy[size] = 0;
    xrtFree(progress->prefix);
    progress->prefix = copy;
    progress->prefixSize = size;
    return 1;
}

int xprog_set_width(void *handle, int width) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL || width < 10 || width > 100)
        return xprog_error("progress width must be 10..100");
    progress->width = width;
    return 1;
}

static int xprog_ratio(const XProg *progress, int scale) {
    long long whole, remainder;
    int low = 0, high = scale;
    if (progress->current >= progress->total) return scale;
    whole = progress->total / scale;
    remainder = progress->total % scale;
    /* 比较 ceil(total * k / scale)，避免乘法溢出和浮点阈值误差。 */
    while (low < high) {
        int middle = low + (high - low + 1) / 2;
        long long threshold = whole * middle +
            (remainder * middle + scale - 1) / scale;
        if (progress->current >= threshold) low = middle;
        else high = middle - 1;
    }
    return low;
}

int xprog_percent(void *handle) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL) { xprog_error("progress bar is destroyed"); return -1; }
    return xprog_ratio(progress, 100);
}

int xprog_is_done(void *handle) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL) return xprog_error("progress bar is destroyed");
    return progress->current == progress->total;
}

int64_t xprog_current(void *handle) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL) { xprog_error("progress bar is destroyed"); return 0; }
    return progress->current;
}

int64_t xprog_total(void *handle) {
    XProg *progress = (XProg *)handle;
    xrtClearError();
    if (progress == NULL) { xprog_error("progress bar is destroyed"); return 0; }
    return progress->total;
}

void *xprog_render(void *handle) {
    XProg *progress = (XProg *)handle;
    char *output;
    size_t position, capacity;
    int filled, i, size;
    xrtClearError();
    if (progress == NULL) { xprog_error("progress bar is destroyed"); return NULL; }
    capacity = progress->prefixSize + (size_t)progress->width + 100;
    output = (char *)xrtMalloc(capacity);
    if (output == NULL) return NULL;
    memcpy(output, progress->prefix, progress->prefixSize);
    position = progress->prefixSize;
    output[position++] = ' ';
    output[position++] = '[';
    filled = xprog_ratio(progress, progress->width);
    for (i = 0; i < progress->width; ++i)
        output[position++] = i < filled ? '=' : i == filled ? '>' : ' ';
    size = snprintf(output + position, capacity - position,
                    "] %3d%% (%lld/%lld)", xprog_ratio(progress, 100),
                    progress->current, progress->total);
    if (size < 0 || (size_t)size >= capacity - position) {
        xrtFree(output);
        xprog_error("progress render failed");
        return NULL;
    }
    return output;
}
