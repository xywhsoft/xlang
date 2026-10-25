#ifndef XL_EXTLIB_CSS_NATIVE_H
#define XL_EXTLIB_CSS_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CSS 正文须为有效 C 字符串；NULL 报 XERR_ARGUMENT，"" 是合法空样式表。 */
/* 文本结果由调用方持有，以 xrtFree 释放；NULL 表示错误。 */
void* xcss_get(const char* css, const char* selector, const char* property);
int64_t xcss_count_rules(const char* css);
int64_t xcss_count_props(const char* css, const char* selector);
void* xcss_selector_at(const char* css, int64_t index);
bool xcss_validate(const char* css);
void* xcss_strip_comments(const char* css);

#ifdef __cplusplus
}
#endif

#endif
