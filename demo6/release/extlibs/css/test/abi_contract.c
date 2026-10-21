#include <stdint.h>
#include "../include/css_native.h"

/* 查询索引与计数使用 XLang int 的 C ABI。 */
extern int64_t xcss_count_rules(const char* css);
extern int64_t xcss_count_props(const char* css, const char* selector);
extern void* xcss_selector_at(const char* css, int64_t index);
