/* 独立的 C 对照范例：覆盖配置、延迟插值、规范写回，不访问文件。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char source[] = "scheme=http\n[service]\nhost=localhost\nport=8080\nwelcome=old\nnote=unchanged\n"
        "welcome=hello\n    world\nendpoint=${.scheme}://${host}:${port}\n";
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bStrictDuplicates = false;
    options.bInterpolation = true;
    xrtiniconfig* config = xrtIniParseMemory(source, sizeof(source) - 1, &options);
    xrtiniconfig* restored = NULL;
    xbuffer* output = NULL;
    str endpoint = NULL, welcome = NULL, note = NULL;
    int status = 1;
    if (!config || xrtIniError(config)->iCode != XRT_INI_OK ||
        !xrtIniSet(config, (str)"service", (str)"port", (str)"9090")) goto done;
    output = xrtIniStringify(config);
    if (!output) goto done;
    options.bStrictDuplicates = true;
    restored = xrtIniParseMemory(output->Data, output->Size, &options);
    if (!restored || xrtIniError(restored)->iCode != XRT_INI_OK) goto done;
    endpoint = xrtIniGet(restored, (str)"service", (str)"endpoint");
    welcome = xrtIniGet(restored, (str)"service", (str)"welcome");
    note = xrtIniGet(restored, (str)"service", (str)"note");
    if (!endpoint || !welcome || !note || strcmp(endpoint, "http://localhost:9090") != 0 ||
        strcmp(welcome, "hello\nworld") != 0 || strcmp(note, "unchanged") != 0) goto done;
    printf("%s\n%s\n%s\n", endpoint, welcome, note);
    status = 0;
done:
    xrtFree(endpoint);
    xrtFree(welcome);
    xrtFree(note);
    xrtBufferDestroy(output);
    xrtIniDestroy(restored);
    xrtIniDestroy(config);
    return status;
}

/* 单文件可 xl run/build；正式包仍通过清单分离编译。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
