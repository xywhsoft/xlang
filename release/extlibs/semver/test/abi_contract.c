#include <stdint.h>
#include "../semver_native.h"

/* 整数结果按 XLang int 的平台 ABI 暴露，避免 LP64 的 long long 冲突。 */
extern int64_t xsemver_major(const char *version);
extern int64_t xsemver_minor(const char *version);
extern int64_t xsemver_patch(const char *version);
extern int64_t xsemver_compare(const char *a, const char *b);
