#ifndef XL_EXTLIB_QR_NATIVE_H
#define XL_EXTLIB_QR_NATIVE_H

#include <stddef.h>
#include <stdint.h>

struct xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 所有长度均为 UTF-8 字节数；文本不能含 NUL。矩阵用 xrtValueRelease。 */
struct xvalue *xqr_matrix_n(const char *text, size_t length,
    const char *level, size_t levelSize);
int64_t xqr_version_n(const char *text, size_t length,
    const char *level, size_t levelSize);

/* 三种渲染结果均为独立 XRT 字符串，调用方用 xrtFree。 */
char *xqr_ascii_n(const char *text, size_t length,
    const char *level, size_t levelSize,
    const char *dark, size_t darkSize,
    const char *light, size_t lightSize);
char *xqr_utf8_n(const char *text, size_t length,
    const char *level, size_t levelSize);
char *xqr_svg_n(const char *text, size_t length,
    const char *level, size_t levelSize,
    int64_t border, int64_t scale);

#ifdef __cplusplus
}
#endif
#endif
