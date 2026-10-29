/* 与 calendar_guard.xl 使用相同配置、边界和五行固定输出。 */
#include "../xrt_toml_internal.h"

static str stamp(xvalue* config, xtime time, xrttomlerror* error)
{
    xvalue* value = xrtValueTime(time);
    if (value == NULL || !xrtValueObjectSetTake(config, XRT_STR_LITERAL("stamp"), &value)) {
        xrtValueRelease(value); return NULL;
    }
    xbuffer* text = xrtTomlStringify(config, NULL, error);
    xvalue* read_back = text != NULL ? xrtTomlParseMemory(text->Data, text->Size, NULL, error) : NULL;
    xrttomldatetime parts;
    str result = read_back != NULL && xrtTomlGetDateTime(xrtValueObjectGet(read_back,
        XRT_STR_LITERAL("stamp")), &parts) && parts.iKind == XRT_TOML_OFFSET_DATETIME ?
        xrtTomlDateTimeToString(&parts) : NULL;
    xrtValueRelease(read_back); xrtBufferDestroy(text);
    return result;
}

int main(void)
{
    static const char source[] = "stamp = 1970-01-01T00:00:00Z\n";
    static const xtime times[] = {1,-INT64_C(62167219200000000),INT64_C(253402300799999999)};
    static const cstr labels[] = {"stamp","year_zero","year_last"};
    static const xtime invalid[] = {INT64_MIN,-INT64_C(62167219200000001),INT64_C(253402300800000000),INT64_MAX};
    xrttomlerror error;
    xvalue* config = xrtTomlParseMemory(source, sizeof(source)-1, NULL, &error);
    bool ok = config != NULL;
    for (unsigned i = 0; ok && i != 3; ++i) {
        str text = stamp(config,times[i],&error);
        ok = text != NULL;
        if (ok) printf("%s=%s\n",labels[i],text);
        xrtFree(text);
    }
    unsigned rejected = 0;
    for (unsigned i = 0; ok && i != 4; ++i) {
        str text = stamp(config,invalid[i],&error);
        ok = text == NULL && error.iCode == XRT_TOML_ERROR_FORMAT;
        if (ok) ++rejected;
        xrtFree(text);
    }
    str restored = ok ? stamp(config,0,&error) : NULL;
    ok = restored != NULL && strcmp(restored,"1970-01-01T00:00:00Z") == 0;
    if (ok) printf("rejected=%u\nrestored=true\n",rejected);
    xrtFree(restored); xrtValueRelease(config);
    return ok ? 0 : 1;
}

#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
