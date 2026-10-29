#include <stdint.h>
#include "../include/rtree_native.h"

/* XLang int 的 ID、查询数量和总数必须使用 int64_t。 */
extern bool xrtree_insert(void *handle, int64_t id,
    double minx, double miny, double maxx, double maxy);
extern int64_t xrtree_search_count(void *handle,
    double minx, double miny, double maxx, double maxy);
extern bool xrtree_remove(void *handle, int64_t id);
extern int64_t xrtree_count(void *handle);
