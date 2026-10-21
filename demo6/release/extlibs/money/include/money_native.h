#ifndef XL_EXTLIB_MONEY_NATIVE_H
#define XL_EXTLIB_MONEY_NATIVE_H

#include <stddef.h>
#include <stdint.h>

typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 整数金额：成功时写 out，失败时 out 保持不变并设置 XRT 错误。 */
int xm_add_checked(int64_t a, int64_t b, int64_t *out);
int xm_sub_checked(int64_t a, int64_t b, int64_t *out);
int xm_mul_checked(int64_t a, int64_t b, int64_t *out);
int xm_scale_checked(int64_t amount, int64_t numerator, int64_t denominator, int64_t *out);
int xm_rate_checked(int64_t amount, double rate, int64_t *out);

/* format 返回 XRT 拥有的字符串；调用方用 xrtFree 释放。symbol 为借用指针。 */
void *xm_format(int64_t amount, const char *code, size_t length);
const char *xm_symbol(const char *code, size_t length);
int64_t xm_exponent(const char *code, size_t length);

/* XLang extern 桥接；成功为整数 xvalue，失败为 null。 */
xvalue *xm_add(int64_t a, int64_t b);
xvalue *xm_sub(int64_t a, int64_t b);
xvalue *xm_mul(int64_t a, int64_t b);
xvalue *xm_scale(int64_t amount, int64_t numerator, int64_t denominator);
xvalue *xm_rate(int64_t amount, double rate);
xvalue *xm_split(int64_t amount, int64_t n);

#ifdef __cplusplus
}
#endif

#endif
