#ifndef XLANG_EXTLIB_COLOR_NATIVE_H
#define XLANG_EXTLIB_COLOR_NATIVE_H
#include "color.h"
XRT_EXTERN_C_BEGIN
/* 语言 ABI 的 point 对应 void*。返回拥有的 XRT 字符串，交给 take_cstr 或 xrtFree。
 * NULL 仅代表分配失败并设置 XRT error，不能当普通空字符串吞掉。 */
void* xcolor_hex_text(int64_t r, int64_t g, int64_t b);
void* xcolor_hex_a_text(int64_t r, int64_t g, int64_t b, int64_t a);
void* xcolor_normalize_text(const char *text);
void* xcolor_from_hsv_text(double h, double s, double v);
void* xcolor_from_hsl_text(double h, double s, double l);
void* xcolor_mix_text(const char *a, const char *b, double t);
void* xcolor_from_name_text(const char *name);
void* xcolor_to_name_text(const char *text);
XRT_EXTERN_C_END
#endif
