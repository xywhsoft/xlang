#include <stdint.h>
#include "../include/css_native.h"

/* 查询索引与计数使用 XLang int 的 C ABI。 */
extern int64_t xcss_count_rules(const char* css);
extern int64_t xcss_count_props(const char* css, const char* selector);
extern char* xcss_get(const char* css, const char* selector, const char* property);
extern char* xcss_selector_at(const char* css, int64_t index);
extern char* xcss_strip_comments(const char* css);
