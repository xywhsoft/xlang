/* 与escaped_credentials.xl同输入/输出，只验证合成密码，不输出其内容。 */
#include "../xrt_netrc_internal.h"

static bool text_is(xvalue* object, cstr key, cstr expected)
{
    xstrview text;
    return xrtValueGetString(xrtValueObjectGet(object, xrtStrView(key)), &text) &&
        text.Size == strlen(expected) && memcmp(text.Data, expected, text.Size) == 0;
}

int main(void)
{
    static const char source[] = "machine \"api host\"\n login \"build bot\"\n"
        " account \"team\\\"A\"\n password \"folder\\\\test # 你好\"\n"
        "macdef \"deploy job\"\n echo ok\n\n";
    xrtnetrcerror error;
    xrtnetrcoptions options;
    xvalue* config = xrtNetrcParseMemory(source, sizeof(source) - 1u, NULL, &error);
    xvalue* restored = NULL;
    xbuffer* wire = config != NULL ? xrtNetrcStringify(config, NULL, &error) : NULL;
    xbuffer* result = NULL;
    int status = 1;
    if (wire == NULL) goto done;
    restored = xrtNetrcParseMemory(wire->Data, wire->Size, NULL, &error);
    if (restored == NULL) goto done;
    xvalue* hosts = xrtValueObjectGet(restored, xrtStrView("hosts"));
    xvalue* auth = xrtValueObjectGet(hosts, xrtStrView("api host"));
    xvalue* macros = xrtValueObjectGet(restored, xrtStrView("macros"));
    xvalue* lines = xrtValueObjectGet(macros, xrtStrView("deploy job"));
    xstrview line;
    if (!text_is(auth, "login", "build bot") || !text_is(auth, "account", "team\"A") ||
        !text_is(auth, "password", "folder\\test # 你好") || xrtValueCount(lines) != 1u ||
        !xrtValueGetString(xrtValueArrayGet(lines, 0u), &line) ||
        line.Size != 8u || memcmp(line.Data, " echo ok", 8u) != 0) goto done;
    puts("quoted host: true\nquoted account: true\npassword roundtrip: true\nquoted macro: true");
    xrtNetrcOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = wire->Size - 1u;
    result = xrtNetrcStringify(config, &options, &error);
    if (result != NULL || error.iCode != XRT_NETRC_ERROR_LIMIT) goto done;
    result = xrtNetrcStringify(config, NULL, &error);
    if (result == NULL || result->Size != wire->Size || memcmp(result->Data, wire->Data, wire->Size) != 0) goto done;
    puts("output limit: true");
    status = 0;
done:
    xrtBufferDestroy(result);
    xrtBufferDestroy(wire);
    xrtValueRelease(restored);
    xrtValueRelease(config);
    return status;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
