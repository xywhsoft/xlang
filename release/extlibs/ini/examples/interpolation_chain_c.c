/* 与 XLang 范例使用相同配置、修改顺序和黄金输出，不需要文件。 */
#define XRT_MODULE_ALL
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

static bool print_value(xrtiniconfig* config, const char* label, const char* expected)
{
    char* value = xrtIniGet(config, (str)"service", (str)"api");
    bool ok = value && strcmp(value, expected) == 0;
    if (ok) printf("%s: %s\n", label, value);
    xrtFree(value);
    return ok;
}

int main(void)
{
    const char input[] = "[service]\nhost=localhost\nport=8080\nbase=http://${host}:${port}\napi=${base}/v1\n";
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    options.tLimits.iMaxDepth = 4;
    xrtiniconfig* config = xrtIniParseMemory(input, sizeof(input) - 1, &options);
    char* rejected = NULL;
    int result = 1;
    if (!config || xrtIniError(config)->iCode != XRT_INI_OK ||
        !print_value(config, "url", "http://localhost:8080/v1") ||
        !xrtIniSet(config, (str)"service", (str)"port", (str)"9090") ||
        !print_value(config, "updated", "http://localhost:9090/v1") ||
        !xrtIniSet(config, (str)"service", (str)"host", (str)"${api}")) goto done;
    rejected = xrtIniGet(config, (str)"service", (str)"api");
    if (rejected || xrtIniError(config)->iCode != XRT_INI_ERROR_INTERPOLATION) goto done;
    puts("cycle: rejected");
    if (!xrtIniSet(config, (str)"service", (str)"host", (str)"localhost") ||
        !print_value(config, "recovered", "http://localhost:9090/v1")) goto done;
    result = ferror(stdout) ? 1 : 0;
done:
    xrtFree(rejected);
    xrtIniDestroy(config);
    return result;
}

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
