#ifndef XL_EXTLIB_BARCODE_NATIVE_H
#define XL_EXTLIB_BARCODE_NATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 输入均为 ASCII 数字字节串，size 是真实字节长度，不能用 strlen 截断。 */
int64_t xb_checksum_ean(const char *digits, size_t size);
bool xb_validate_ean(const char *digits, size_t size);
xvalue *xb_ean13(const char *digits, size_t size);
xvalue *xb_upc_a(const char *digits, size_t size);

/* 返回的 XRT 内存由调用方用 xrtFree 释放；失败返回 NULL。 */
void *xb_svg(const char *digits, size_t size, bool upc, int64_t height, int64_t scale);
void *xb_ascii(const char *digits, size_t size, bool upc);

#ifdef __cplusplus
}
#endif

#endif
