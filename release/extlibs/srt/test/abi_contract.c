#include <stdint.h>
#include "../include/srt_native.h"

/* 毫秒时间参数和结果必须与 XLang int 的 C ABI 一致。 */
extern char *xsrt_build(xvalue *cues);
extern char *xsrt_build_vtt(xvalue *cues);
extern char *xsrt_format_time(int64_t milliseconds);
extern int64_t xsrt_parse_time(const char *timestamp);
extern int64_t xsrt_parse_time_n(const char *timestamp, size_t size);
