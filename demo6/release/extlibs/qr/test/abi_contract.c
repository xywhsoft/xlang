#include <stdint.h>
#include "../include/qr_native.h"

/* 版本号和 SVG 数值参数与 XLang int 的 C ABI 一致。 */
extern int64_t xqr_version_n(const char *text, size_t length,
    const char *level, size_t levelSize);
extern char *xqr_svg_n(const char *text, size_t length,
    const char *level, size_t levelSize, int64_t border, int64_t scale);
