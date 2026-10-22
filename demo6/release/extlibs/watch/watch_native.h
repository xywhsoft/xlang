#ifndef XL_WATCH_NATIVE_H
#define XL_WATCH_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xwatch_create(void);
void xwatch_destroy(void *handle);
int64_t xwatch_add(void *handle, const char *path);
bool xwatch_remove(void *handle, int64_t index);
/* 结果由调用方 xrtFree；NULL 为错误，空串为无变化。 */
void *xwatch_poll(void *handle);
int64_t xwatch_count(void *handle);
/* 借用路径；添加其他项时保持有效，删除对应项、销毁或并发操作后失效。 */
const char *xwatch_path(void *handle, int64_t index);
bool xwatch_exists(const char *path);

#ifdef __cplusplus
}
#endif
#endif
