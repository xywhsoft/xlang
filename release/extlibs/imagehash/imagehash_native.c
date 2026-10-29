/* 感知哈希只计算像素特征；解码、UTF-8 文件访问和缩放复用已有扩展库。 */
#include "include/imagehash_native.h"
#include "../stb_image/include/stb_image_native.h"
#include "../stb_image_resize2/include/stb_image_resize2_native.h"

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#include "xrt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define XIHASH_PI 3.14159265358979323846

static xonce xihash_error_once = XRT_ONCE_INIT;
static xthreadkey *xihash_error_key;
static xonce xihash_basis_once = XRT_ONCE_INIT;
static int32_t xihash_basis[8][32];

/* DCT 基底与图片无关；进程内只计算一次，并由 XRT 保证并发首次调用安全。 */
static bool xihash_basis_init(ptr unused) {
    int u, x;
    (void)unused;
    for (u = 0; u < 8; ++u) {
        for (x = 0; x < 32; ++x) {
            xihash_basis[u][x] = (int32_t)llround(
                cos((2.0 * x + 1.0) * u * XIHASH_PI / 64.0) * 65536.0);
        }
    }
    return true;
}

static bool xihash_error_key_init(ptr unused) {
    (void)unused;
    xihash_error_key = xrtThreadKeyCreate(xrtFree);
    return xihash_error_key != NULL;
}

/* TCC JIT 的静态 TLS 不可靠；错误文本由 XRT 线程键隔离并在线程退出时释放。 */
static char *xihash_error_buffer(bool create) {
    char *buffer;
    if (!xrtOnce(&xihash_error_once, xihash_error_key_init, NULL)) return NULL;
    buffer = (char *)xrtThreadKeyGet(xihash_error_key);
    if (buffer != NULL || !create) return buffer;
    buffer = (char *)xrtMalloc(128);
    if (buffer == NULL) return NULL;
    buffer[0] = '\0';
    if (!xrtThreadKeySet(xihash_error_key, buffer)) {
        xrtFree(buffer);
        return NULL;
    }
    return buffer;
}

static void xihash_set_error(const char *text) {
    char *buffer = xihash_error_buffer(text != NULL && text[0] != '\0');
    if (buffer != NULL) snprintf(buffer, 128, "%s", text);
}

static void xihash_dependency_error(const char *fallback) {
    const xerror *cause = xrtGetError();
    if (cause != NULL && xrtErrorKind(cause) == XERR_MEMORY) {
        xihash_set_error("out of memory");
        return;
    }
    xrtClearError();
    xihash_set_error(fallback);
}

static xbuffer *xihash_pixels(const char *path, int width, int height) {
    void *image;
    xbuffer *resized;
    if (path == NULL || path[0] == 0) {
        xihash_set_error("image path is empty");
        return NULL;
    }
    image = xstbi_load(path, 1);
    if (image == NULL) {
        xihash_dependency_error("cannot read or decode image");
        return NULL;
    }
    resized = xstbir_resize(xstbi_image_data(image), xstbi_image_byte_size(image),
        xstbi_image_width(image), xstbi_image_height(image), 1,
        width, height, 0, 1);
    xstbi_image_release(image);
    if (resized == NULL) {
        xihash_dependency_error("cannot resize image");
    }
    return resized;
}

static char *xihash_format(uint64_t value) {
    char *result = xrtMalloc(17);
    if (result == NULL) {
        xihash_set_error("out of memory");
        return NULL;
    }
    snprintf(result, 17, "%016llx", (unsigned long long)value);
    xihash_set_error("");
    xrtClearError();
    return result;
}

static uint64_t xihash_average(const unsigned char *pixels) {
    uint64_t result = 0;
    unsigned sum = 0;
    unsigned i;
    for (i = 0; i < 64; ++i) sum += pixels[i];
    for (i = 0; i < 64; ++i) {
        if ((unsigned)pixels[i] * 64u >= sum) result |= UINT64_C(1) << i;
    }
    return result;
}

static uint64_t xihash_difference(const unsigned char *pixels) {
    uint64_t result = 0;
    int x, y;
    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            if (pixels[y * 9 + x] > pixels[y * 9 + x + 1]) {
                result |= UINT64_C(1) << (y * 8 + x);
            }
        }
    }
    return result;
}

static int xihash_compare_int64(const void *a, const void *b) {
    int64_t left = *(const int64_t *)a, right = *(const int64_t *)b;
    return (left > right) - (left < right);
}

static uint64_t xihash_perceptual(const unsigned char *pixels) {
    int64_t rows[8][32], coefficients[64], sorted[63], median;
    uint64_t result = 0;
    int u, v, x, y, i;
    /* 二维 DCT 分成两次一维乘法；系数用定点整数比较，跨编译器不受浮点尾差影响。 */
    for (u = 0; u < 8; ++u) {
        for (y = 0; y < 32; ++y) {
            int64_t sum = 0;
            for (x = 0; x < 32; ++x) sum += pixels[y * 32 + x] * xihash_basis[u][x];
            rows[u][y] = sum;
        }
    }
    for (v = 0; v < 8; ++v) {
        for (u = 0; u < 8; ++u) {
            int64_t sum = 0;
            for (y = 0; y < 32; ++y) sum += rows[u][y] * xihash_basis[v][y];
            coefficients[v * 8 + u] = sum;
        }
    }
    for (i = 1; i < 64; ++i) sorted[i - 1] = coefficients[i];
    qsort(sorted, 63, sizeof(sorted[0]), xihash_compare_int64);
    median = sorted[31];
    for (i = 1; i < 64; ++i) {
        if (coefficients[i] > median) result |= UINT64_C(1) << (i - 1);
    }
    return result;
}

static int xihash_digit(unsigned char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool xihash_parse(const char *text, size_t length, uint64_t *out) {
    uint64_t value = 0;
    size_t i;
    if (text == NULL || length != 16) return false;
    for (i = 0; i < 16; ++i) {
        int digit = xihash_digit((unsigned char)text[i]);
        if (digit < 0) return false;
        value = (value << 4) | (unsigned)digit;
    }
    *out = value;
    return true;
}

bool xihash_available(void) { return true; }
const char *xihash_version(void) { return "imagehash 0.1.0"; }
const char *xihash_last_error(void) {
    char *buffer = xihash_error_buffer(false);
    return buffer != NULL ? buffer : "";
}

char *xihash_ahash(const char *path) {
    xbuffer *pixels;
    uint64_t value;
    xrtClearError();
    pixels = xihash_pixels(path, 8, 8);
    if (pixels == NULL) return NULL;
    value = xihash_average(pixels->Data);
    xrtBufferDestroy(pixels);
    return xihash_format(value);
}

char *xihash_dhash(const char *path) {
    xbuffer *pixels;
    uint64_t value;
    xrtClearError();
    pixels = xihash_pixels(path, 9, 8);
    if (pixels == NULL) return NULL;
    value = xihash_difference(pixels->Data);
    xrtBufferDestroy(pixels);
    return xihash_format(value);
}

char *xihash_phash(const char *path) {
    xbuffer *pixels;
    uint64_t value;
    xrtClearError();
    pixels = xihash_pixels(path, 32, 32);
    if (pixels == NULL) return NULL;
    if (!xrtOnce(&xihash_basis_once, xihash_basis_init, NULL)) {
        xrtBufferDestroy(pixels);
        xihash_set_error("cannot initialize pHash basis");
        return NULL;
    }
    value = xihash_perceptual(pixels->Data);
    xrtBufferDestroy(pixels);
    return xihash_format(value);
}

int xihash_hamming(const char *left, size_t left_length,
    const char *right, size_t right_length) {
    uint64_t a, b, difference;
    int count = 0;
    xrtClearError();
    if (!xihash_parse(left, left_length, &a) ||
        !xihash_parse(right, right_length, &b)) {
        xihash_set_error("invalid hash string");
        return -1;
    }
    difference = a ^ b;
    while (difference != 0) {
        difference &= difference - 1;
        ++count;
    }
    xihash_set_error("");
    xrtClearError();
    return count;
}

bool xihash_valid(const char *hash, size_t length) {
    uint64_t value;
    bool valid;
    xrtClearError();
    xihash_set_error("");
    valid = xihash_parse(hash, length, &value);
    xrtClearError();
    return valid;
}
