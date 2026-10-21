#ifndef XL_EXTLIB_RTREE_NATIVE_H
#define XL_EXTLIB_RTREE_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

struct xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 句柄由 xrtree_destroy 释放；同一句柄不能跨线程并发修改。 */
void *xrtree_create(void);
void xrtree_destroy(void *handle);
bool xrtree_insert(void *handle, int64_t id,
    double minx, double miny, double maxx, double maxy);
struct xvalue *xrtree_search(void *handle,
    double minx, double miny, double maxx, double maxy);
int64_t xrtree_search_count(void *handle,
    double minx, double miny, double maxx, double maxy);
bool xrtree_remove(void *handle, int64_t id);
int64_t xrtree_count(void *handle);
void xrtree_clear(void *handle);

#ifdef __cplusplus
}
#endif
#endif
