#include "xrt.h"
#include "../semver_native.h"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    const char *version = "2.4.1-beta.2+build-5";
    char *pre = xsemver_prerelease(version);
    char *build = xsemver_build(version);
    char *clean = xsemver_clean(" v2.4.1 ");
    if (!pre || !build || !clean) {
        xrtFree(pre);
        xrtFree(build);
        xrtFree(clean);
        return 1;
    }

    printf("version: %s\n", version);
    printf("valid: %s\n", xsemver_valid(version) ? "true" : "false");
    printf("major: %" PRId64 "\n", xsemver_major(version));
    printf("minor: %" PRId64 "\n", xsemver_minor(version));
    printf("patch: %" PRId64 "\n", xsemver_patch(version));
    printf("prerelease: %s\n", pre);
    printf("build: %s\n", build);
    printf("clean: %s\n", clean);
    printf("compare: %" PRId64 "\n", xsemver_compare("2.4.1-beta.2", "2.4.1"));
    printf("compatible: %s\n", xsemver_satisfies("2.4.9", "~2.4.0") ? "yes" : "no");
    printf("upper-bound: %s\n", xsemver_satisfies("2.4.0", "<=2.4.1") ? "yes" : "no");
    printf("interval: %s\n", xsemver_satisfies("2.4.1", "2.4.0  -  2.4.9") ? "yes" : "no");
    xrtFree(pre);
    xrtFree(build);
    xrtFree(clean);
    return 0;
}

/* 单文件范例可直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../semver_native.c"
