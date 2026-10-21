#ifndef XL_EXTLIB_RSS_NATIVE_H
#define XL_EXTLIB_RSS_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 解析结果由调用方 xrtValueRelease；失败返回 XVALUE_NULL。 */
const char *xrss_detect(const char *xml, int64 length);
xvalue *xrss_parse(const char *xml, int64 length);
xvalue *xrss_parse_rss(const char *xml, int64 length);
xvalue *xrss_parse_atom(const char *xml, int64 length);

/* 生成结果是独立的 NUL 结尾缓冲，由调用方 xrtFree。失败返回 NULL。 */
void *xrss_build_rss(xvalue *channel, xvalue *items);
void *xrss_build_atom(xvalue *feed, xvalue *entries);

#ifdef __cplusplus
}
#endif
#endif
