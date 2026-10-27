#ifndef XL_FUZZY_NATIVE_H
#define XL_FUZZY_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 参数须为有效的 NUL 结尾字符串，NULL 报 XERR_ARGUMENT。 */
/* 失败分别返回 -1（编辑距离/比率）、-2（评分）；不匹配评分为 -1。 */
int64_t xfuzzy_levenshtein(const char* a, const char* b);
int64_t xfuzzy_ratio(const char* a, const char* b);
int64_t xfuzzy_match(const char* pattern, const char* text);

/* 返回独立分配的 XRT 字符串，C 调用方使用 xrtFree 释放。 */
char* xfuzzy_best(const char* pattern, const char* candidates);

#ifdef __cplusplus
}
#endif

#endif
