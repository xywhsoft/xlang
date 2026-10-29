#ifndef XLANG_EXTLIB_COLOR_NATIVE_H
#define XLANG_EXTLIB_COLOR_NATIVE_H
#include "color.h"
XRT_EXTERN_C_BEGIN
/* 返回独立的 XRT 字符串；C 调用方用 xrtFree，XLang 以 @owned 接管。
 * NULL 仅代表分配失败并设置 XRT error，不能当普通空字符串吞掉。 */
char* xcolor_hex_text(int64_t r, int64_t g, int64_t b);
char* xcolor_hex_a_text(int64_t r, int64_t g, int64_t b, int64_t a);
char* xcolor_normalize_text(const char *text);
char* xcolor_from_hsv_text(double h, double s, double v);
char* xcolor_from_hsl_text(double h, double s, double l);
char* xcolor_mix_text(const char *a, const char *b, double t);
char* xcolor_from_name_text(const char *name);
char* xcolor_to_name_text(const char *text);
XRT_EXTERN_C_END
#endif
