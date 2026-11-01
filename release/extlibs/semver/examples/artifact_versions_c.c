/* 与artifact_versions.xl同输入/输出；文本结果由调用方xrtFree。 */
#include "../semver_native.c"

int main(void)
{
    char linuxVersion[] = "2.4.0-rc.1+linux.x64.0007";
    const char* windowsVersion = "2.4.0-rc.1+windows.x64.0008";
    char* linuxBuild = NULL;
    char* windowsBuild = NULL;
    int result = 1;
    if (!xsemver_valid(linuxVersion) || !xsemver_valid(windowsVersion) ||
        xsemver_compare(linuxVersion, windowsVersion) != 0) goto done;
    linuxBuild = xsemver_build(linuxVersion);
    windowsBuild = xsemver_build(windowsVersion);
    if (linuxBuild == NULL || windowsBuild == NULL) goto done;
    puts("same precedence: true");
    printf("linux build: %s\nwindows build: %s\n", linuxBuild, windowsBuild);
    if (!xsemver_satisfies(linuxVersion, ">=2.4.0-rc.1 <2.4.0") ||
        !xsemver_satisfies(windowsVersion, ">=2.4.0-rc.1 <2.4.0")) goto done;
    puts("release range: true");
    memset(linuxVersion, 'x', strlen(linuxVersion));
    if (strcmp(linuxBuild, "linux.x64.0007") != 0) goto done;
    puts("owned snapshot: true");
    result = 0;
done:
    xrtFree(windowsBuild);
    xrtFree(linuxBuild);
    return result;
}
