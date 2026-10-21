#ifndef XL_EXTLIB_SITEMAP_NATIVE_H
#define XL_EXTLIB_SITEMAP_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* XML 构建结果归调用方 xrtFree；解析结果归调用方 xrtValueRelease。 */
void *xsm_build(xvalue *urls);
void *xsm_build_index(xvalue *entries);
xvalue *xsm_parse(const char *xml);
xvalue *xsm_parse_index(const char *xml);
xvalue *xsm_parse_n(const char *xml, size_t size);
xvalue *xsm_parse_index_n(const char *xml, size_t size);

xvalue *xsm_robots_parse(const char *text);
xvalue *xsm_robots_parse_n(const char *text, size_t size);
long long xsm_robots_allows(xvalue *robots, const char *agent, const char *path);
long long xsm_robots_allows_n(xvalue *robots, const char *agent,
                             size_t agentSize, const char *path, size_t pathSize);

#ifdef __cplusplus
}
#endif
#endif
