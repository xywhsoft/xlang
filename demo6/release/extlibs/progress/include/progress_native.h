#ifndef XL_EXTLIB_PROGRESS_NATIVE_H
#define XL_EXTLIB_PROGRESS_NATIVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xprog_create(int64_t total);
void xprog_destroy(void *handle);
int xprog_advance(void *handle, int64_t count);
int xprog_set(void *handle, int64_t current);
int xprog_set_prefix_n(void *handle, const char *prefix, size_t size);
int xprog_set_width(void *handle, int width);
int xprog_percent(void *handle);
int xprog_is_done(void *handle);
int64_t xprog_current(void *handle);
int64_t xprog_total(void *handle);
/* 独立缓冲；调用方使用 xrtFree。 */
char *xprog_render(void *handle);

#ifdef __cplusplus
}
#endif
#endif
