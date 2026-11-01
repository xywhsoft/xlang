/* 同 XLang 范例：失败后直接复用原配置，不修改全局环境或写文件。 */
#define XRT_MODULE_ALL
#include "../include/xrt_toml.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char source[] = "items = [{name = \"worker\", retries = 3}, 0]\n";
    xrttomloptions options;
    xrttomlerror error;
    xvalue* config = xrtTomlParseMemory(source, sizeof(source) - 1u, NULL, &error);
    xrtTomlOptionsInit(&options); options.tLimits.iMaxOutputBytes = 9;
    xbuffer* output = config != NULL ? xrtTomlStringify(config, &options, &error) : NULL;
    bool ok = config != NULL && output == NULL && error.iCode == XRT_TOML_ERROR_LIMIT;
    xrtBufferDestroy(output);
    output = ok ? xrtTomlStringify(config, NULL, &error) : NULL;
    xvalue* restored = output != NULL ? xrtTomlParseMemory(output->Data, output->Size, NULL, &error) : NULL;
    xvalue* row = xrtValueArrayGet(xrtValueObjectGet(restored, xrtStrView("items")), 0);
    xstrview name;
    int64 retries;
    ok = ok && row != NULL && xrtValueGetString(xrtValueObjectGet(row, xrtStrView("name")), &name) &&
        name.Size == 6u && memcmp(name.Data, "worker", 6u) == 0 &&
        xrtValueGetInt(xrtValueObjectGet(row, xrtStrView("retries")), &retries) && retries == 3;
    if (ok) puts("quota: rejected\ninput: preserved\nretry: worker/3");
    else fprintf(stderr, "TOML output-limit example failed: %s\n", error.sMessage);
    xrtValueRelease(restored); xrtBufferDestroy(output); xrtValueRelease(config);
    return ok ? 0 : 1;
}

#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
