#ifndef XL_EXTLIB_SRT_NATIVE_H
#define XL_EXTLIB_SRT_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include <stdint.h>
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 解析结果由调用方 xrtValueRelease；无效文档返回 XVALUE_NULL。 */
xvalue *xsrt_parse(const char *text);
xvalue *xsrt_parse_vtt(const char *text);
/* 长度入口接受非 NUL 结尾缓冲；内嵌 NUL 报错。 */
xvalue *xsrt_parse_n(const char *text, size_t size);
xvalue *xsrt_parse_vtt_n(const char *text, size_t size);

/* 生成结果是独立 UTF-8 缓冲，由调用方 xrtFree。 */
char *xsrt_build(xvalue *cues);
char *xsrt_build_vtt(xvalue *cues);
char *xsrt_format_time(int64_t milliseconds);
int64_t xsrt_parse_time(const char *timestamp);
int64_t xsrt_parse_time_n(const char *timestamp, size_t size);

#ifdef __cplusplus
}
#endif
#endif
