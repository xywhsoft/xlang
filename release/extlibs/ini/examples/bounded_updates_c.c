/* 与 XLang 范例同样限额、错误分类与输出；所有权由 C 显式释放。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char title[] = "\xe4\xbd\xa0\xe5\xa5\xbd";
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.tLimits.iMaxItemBytes = 6;
    xrtiniconfig* config = xrtIniCreate(&options);
    xrtiniconfig* restored = NULL;
    xbuffer* text = NULL;
    str stored = NULL;
    int64 port = 0;
    int status = 1;
    if (!config || !xrtIniSet(config, (str)"web", (str)"title", (str)title)) goto done;
    if (xrtIniSet(config, (str)"web", (str)"title", (str)"\xe4\xbd\xa0\xe5\xa5\xbd!") ||
        xrtIniError(config)->iCode != XRT_INI_ERROR_LIMIT) goto done;
    puts("oversized update rejected");
    stored = xrtIniGet(config, (str)"web", (str)"title");
    if (!stored || strcmp(stored, title) != 0) goto done;
    puts(stored);
    if (!xrtIniSet(config, (str)"web", (str)"port", (str)"8080")) goto done;
    text = xrtIniStringify(config);
    if (!text) goto done;
    restored = xrtIniParseMemory(text->Data, text->Size, &options);
    if (!restored || xrtIniError(restored)->iCode != XRT_INI_OK ||
        !xrtIniGetInt(restored, (str)"web", (str)"port", &port) || port != 8080) goto done;
    printf("%lld\n", (long long)port);
    puts("bounded configuration round-trip: OK");
    status = 0;
done:
    xrtFree(stored);
    xrtBufferDestroy(text);
    xrtIniDestroy(restored);
    xrtIniDestroy(config);
    return status;
}

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
