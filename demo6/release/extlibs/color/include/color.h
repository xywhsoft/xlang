#ifndef XLANG_EXTLIB_COLOR_H
#define XLANG_EXTLIB_COLOR_H
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include <stdint.h>

#define XCOLOR_HEX_SIZE 10u
#define XCOLOR_NAME_SIZE 24u

XRT_EXTERN_C_BEGIN
/* 输入是借用的 NUL 结尾文本，支持带或不带 # 的 3/4/6/8 位十六进制。
 * 正常不可解析返回 false/-1/空输出，不设置错误；每次调用清除旧 XRT 错误。
 * C 文本输出由调用者提供并持有，支持与输入重叠。capacity 包含结尾 NUL；
 * 容量不足设置 XERR_RANGE，NULL/零容量输出设置 XERR_ARGUMENT。
 * 没有共享可变状态；合法数据计算不分配。不同线程使用各自的输出缓冲。
 */
bool xcolor_valid(const char *text);
int64_t xcolor_red(const char *text);
int64_t xcolor_green(const char *text);
int64_t xcolor_blue(const char *text);
int64_t xcolor_alpha(const char *text);
bool xcolor_hex(int64_t r, int64_t g, int64_t b, char *output, size_t capacity);
bool xcolor_hex_a(int64_t r, int64_t g, int64_t b, int64_t a, char *output, size_t capacity);
bool xcolor_normalize(const char *text, char *output, size_t capacity);
double xcolor_hue(const char *text);
double xcolor_saturation(const char *text);
double xcolor_value(const char *text);
double xcolor_lightness(const char *text);
bool xcolor_from_hsv(double h, double s, double v, char *output, size_t capacity);
bool xcolor_from_hsl(double h, double s, double l, char *output, size_t capacity);
double xcolor_luminance(const char *text);
double xcolor_contrast(const char *a, const char *b);
bool xcolor_is_dark(const char *text);
bool xcolor_is_light(const char *text);
bool xcolor_mix(const char *a, const char *b, double t, char *output, size_t capacity);
bool xcolor_from_name(const char *name, char *output, size_t capacity);
bool xcolor_to_name(const char *text, char *output, size_t capacity);
XRT_EXTERN_C_END
#endif
