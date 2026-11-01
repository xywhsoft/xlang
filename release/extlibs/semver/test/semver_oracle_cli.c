#include "../semver_native.h"
#include "xrt.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[2048];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *first = strchr(line, '\t');
        char *second = first != NULL ? strchr(first + 1, '\t') : NULL;
        char *end = strchr(line, '\n');
        if (first != line + 1 || second == NULL || end == NULL || second > end) return 2;
        *first++ = '\0';
        *second++ = '\0';
        *end = '\0';
        /* 测试协议只接收逐行 S=范围、V=有效性、C=优先级；不进入发布库。 */
        if (line[0] == 'S') puts(xsemver_satisfies(first, second) ? "1" : "0");
        else if (line[0] == 'V' && second[0] == '\0')
            puts(xsemver_valid(first) ? "1" : "0");
        else if (line[0] == 'C')
            printf("%" PRId64 "\n", xsemver_compare(first, second));
        else if (line[0] == 'M' && second[0] == '\0') {
            /* 元数据不含tab/换行，九个公开结果可用简单逐行协议完整对照。 */
            char* pre = xsemver_prerelease(first);
            char* build = xsemver_build(first);
            char* clean = xsemver_clean(first);
            if (pre == NULL || build == NULL || clean == NULL) {
                xrtFree(pre); xrtFree(build); xrtFree(clean);
                return 3;
            }
            printf("%d\t%d\t%" PRId64 "\t%" PRId64 "\t%" PRId64 "\t%" PRId64 "\t%s\t%s\t%s\n",
                (int)xsemver_valid(first), (int)xsemver_satisfies(first, ">=0.0.0"),
                xsemver_major(first), xsemver_minor(first), xsemver_patch(first),
                xsemver_compare(first, first), pre, build, clean);
            xrtFree(pre); xrtFree(build); xrtFree(clean);
        }
        else return 2;
    }
    return ferror(stdin) ? 2 : 0;
}
