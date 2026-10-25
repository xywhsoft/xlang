#ifndef XL_EXTLIB_ROARING_NATIVE_H
#define XL_EXTLIB_ROARING_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

struct xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 输入域为 uint32；add 成功包含重复插入，remove 返回是否真正删除。 */
void *xroaring_create(void);
void xroaring_destroy(void *handle);
bool xroaring_add(void *handle, int64_t value);
bool xroaring_remove(void *handle, int64_t value);
bool xroaring_contains(void *handle, int64_t value);
int64_t xroaring_count(void *handle);
void xroaring_clear(void *handle);
int64_t xroaring_min(void *handle);
int64_t xroaring_max(void *handle);
/* 返回由调用方 xrtValueRelease 的升序数组快照；失败返回 null 值并设置 XRT 错误。 */
struct xvalue *xroaring_to_array(void *handle);

#ifdef __cplusplus
}
#endif

#endif
