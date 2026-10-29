#ifndef XL_SEMVER_NATIVE_H
#define XL_SEMVER_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 输入通常须以 NUL 结尾；字符串结果由 XRT 分配，调用方用 xrtFree 释放。 */
bool xsemver_valid(const char *version);
int64_t xsemver_major(const char *version);
int64_t xsemver_minor(const char *version);
int64_t xsemver_patch(const char *version);
char *xsemver_prerelease(const char *version);
char *xsemver_build(const char *version);
char *xsemver_clean(const char *version);
int64_t xsemver_compare(const char *a, const char *b);
bool xsemver_satisfies(const char *version, const char *range);

#ifdef __cplusplus
}
#endif
#endif
