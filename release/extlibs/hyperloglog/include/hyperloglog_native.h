#ifndef XL_HYPERLOGLOG_NATIVE_H
#define XL_HYPERLOGLOG_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* xhll_create(int precision);
void xhll_destroy(void* handle);
/* length=-1 使用 strlen；显式长度支持含 NUL 的二进制数据。 */
int xhll_add(void* handle, const char* data, long long length);
int64_t xhll_count(void* handle);
int xhll_clear(void* handle);
int xhll_merge(void* destination, void* source);
int xhll_precision(void* handle);
int64_t xhll_added(void* handle);

#ifdef __cplusplus
}
#endif

#endif
