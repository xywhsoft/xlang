#ifndef XL_EXTLIB_CRONTAB_NATIVE_H
#define XL_EXTLIB_CRONTAB_NATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 五字段 cron；时间戳为 Unix 秒，按进程本地时区匹配。 */
bool xcron_is_valid(const char *text, size_t size);
bool xcron_matches(const char *text, size_t size, int64_t timestamp);
int64_t xcron_next(const char *text, size_t size, int64_t after);
/* 返回独立的 XRT 堆字符串，调用方用 xrtFree 释放。 */
char *xcron_describe(const char *text, size_t size);

#ifdef __cplusplus
}
#endif

#endif
