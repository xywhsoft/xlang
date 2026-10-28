#include "../semver_native.h"
#include <stdio.h>

/* 候选只在本次调用内借用，筛选结果继续借用输入数组，不增加分配。 */
static const char* best_version(const char* const* versions, size_t count, const char* rule)
{
    const char* best = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (xsemver_satisfies(versions[i], rule) &&
            (best == NULL || xsemver_compare(versions[i], best) > 0)) best = versions[i];
    }
    return best != NULL ? best : "none";
}

int main(void)
{
    const char* versions[] = {"0.0.0-alpha", "1.2.0", "1.2.9", "1.3.0-0", "1.3.0-beta", "1.3.0", "2.0.0-alpha"};
    size_t count = sizeof(versions) / sizeof(versions[0]);
    printf("stable 1.2: %s\n", best_version(versions, count, "1.2"));
    printf("beta channel: %s\n", best_version(versions, count, ">=1.3.0-beta <1.4.0"));
    printf("next major: %s\n", best_version(versions, count, "^1.0.0 >=2.0.0-alpha"));
    printf("zero prerelease: %s\n", best_version(versions, count, "* >=0.0.0-alpha <0.0.0"));
    printf("wildcard upper: %s\n", best_version(versions, count, "<=*.*"));
    printf("empty branch union: %s\n", best_version(versions, count, "<* || 1.2"));
    printf("open range: %s\n", best_version(versions, count, "* - *"));
    return 0;
}

/* 独立 C 对照范例；名称不与 ranges.xl 构成同名 companion。 */
#include "../semver_native.c"
