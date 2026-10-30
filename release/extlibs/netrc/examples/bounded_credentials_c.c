/* 与 bounded_credentials.xl 同输入、预算、断言和输出，不记录密码。 */
/* 本例内嵌下方实现，仅启用它使用的 XRT 模块，不依赖 MODULE_ALL。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "../include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

static bool text_is(xvalue* object, cstr key, cstr expected)
{
    xstrview text;
    return xrtValueGetString(xrtValueObjectGet(object, xrtStrView(key)), &text) &&
        text.Size == strlen(expected) && memcmp(text.Data, expected, text.Size) == 0;
}

int main(void)
{
    static const char source[] = "machine api.example login build password sample\n"
        "default login guest\nmacdef ping\n echo alive\n\n";
    xrtnetrcoptions options;
    xrtnetrcerror error;
    xrtNetrcOptionsInit(&options);
    options.tLimits.iMaxInputBytes = 512;
    options.tLimits.iMaxOutputBytes = 512;
    options.tLimits.iMaxItemBytes = 16;
    options.tLimits.iMaxEntries = 4;
    options.tLimits.iMaxNodes = 7;
    xvalue* config = xrtNetrcParseMemory(source, sizeof(source) - 1, &options, &error);
    xvalue* restored = NULL;
    xbuffer* wire = NULL;
    xbuffer* again = NULL;
    int status = 1;
    if (config == NULL) goto done;
    xvalue* hosts = xrtValueObjectGet(config, xrtStrView("hosts"));
    xvalue* macros = xrtValueObjectGet(config, xrtStrView("macros"));
    if (xrtValueCount(hosts) != 2 || xrtValueCount(macros) != 1 ||
        !text_is(xrtValueObjectGet(hosts, xrtStrView("api.example")), "login", "build") ||
        !text_is(xrtValueObjectGet(hosts, xrtStrView("default")), "login", "guest")) goto done;
    puts("hosts: 2\nmacros: 1\nselected login: build\nfallback login: guest");
    wire = xrtNetrcStringify(config, &options, &error);
    if (wire == NULL) goto done;
    restored = xrtNetrcParseMemory(wire->Data, wire->Size, &options, &error);
    if (restored == NULL) goto done;
    hosts = xrtValueObjectGet(restored, xrtStrView("hosts"));
    macros = xrtValueObjectGet(restored, xrtStrView("macros"));
    xvalue* lines = xrtValueObjectGet(macros, xrtStrView("ping"));
    xstrview line;
    if (!text_is(xrtValueObjectGet(hosts, xrtStrView("api.example")), "password", "sample") ||
        xrtValueCount(lines) != 1 || !xrtValueGetString(xrtValueArrayGet(lines, 0), &line) ||
        line.Size != 11 || memcmp(line.Data, " echo alive", 11) != 0) goto done;
    puts("bounded roundtrip: true");
    options.tLimits.iMaxItemBytes = 7;
    xvalue* rejected = xrtNetrcParseMemory("macdef m\n12345678\n\n", 19, &options, &error);
    bool limited = rejected == NULL && error.iCode == XRT_NETRC_ERROR_LIMIT;
    xrtValueRelease(rejected);
    if (!limited) goto done;
    puts("macro limit: true");
    options.tLimits.iMaxItemBytes = 16;
    again = xrtNetrcStringify(config, &options, &error);
    if (again == NULL || wire->Size != again->Size ||
        memcmp(wire->Data, again->Data, wire->Size) != 0) goto done;
    puts("source unchanged: true");
    status = 0;
done:
    if (status != 0) fprintf(stderr, "bounded credentials failed: %d %s\n", error.iCode, error.sMessage);
    xrtBufferDestroy(again);
    xrtBufferDestroy(wire);
    xrtValueRelease(restored);
    xrtValueRelease(config);
    return status;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
