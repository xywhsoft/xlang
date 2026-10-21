#include <stdint.h>
#include "../include/hyperloglog_native.h"

/* add 的 c_longlong 保持原样；两个 XLang int 结果使用 int64_t。 */
extern int xhll_add(void* handle, const char* data, long long length);
extern int64_t xhll_count(void* handle);
extern int64_t xhll_added(void* handle);
