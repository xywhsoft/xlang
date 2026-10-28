#ifndef XL_UNITS_NATIVE_H
#define XL_UNITS_NATIVE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* 文本由 XRT 独立分配，调用方使用 xrtFree；失败返回 NULL 并设置 XRT 错误。 */
char* xu_bytes(double value);
char* xu_bytes_decimal(double value);
char* xu_duration(double seconds);
char* xu_duration_short(double seconds);
char* xu_count(double value);

/* 解析失败返回 NaN，同时设置 XRT 错误。 */
double xu_parse_bytes(const char* text, long long length);
double xu_parse_duration(const char* text, long long length);
int xu_is_pow2(int64_t value);

#ifdef __cplusplus
}
#endif
#endif
