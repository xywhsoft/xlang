#define XRT_MODULE_ALL
#include "xrt.h"
#include "../json5_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* config = "{service:'xlang', workers:4, tags:['api',], 标题:'JSON5',}";
    size_t size = strlen(config);
    xvalue* root = xjson5_parse_len(config, size);
    xvalue* selected = NULL;
    xvalue* field;
    str path_text = NULL;
    xstrview text;
    int64 integer;
    int status = 1;
    if (!root || xjson5_has_error()) goto done;
    field = xrtValueObjectGet(root, XRT_STR_LITERAL("service"));
    if (!field || !xrtValueGetString(field, &text) || text.Size != 5 ||
        memcmp(text.Data, "xlang", 5) != 0) goto done;
    printf("service=%.*s\n", (int)text.Size, text.Data);
    field = xrtValueObjectGet(root, XRT_STR_LITERAL("workers"));
    if (!field || !xrtValueGetInt(field, &integer) || integer != 4) goto done;
    printf("workers=%lld\n", (long long)integer);
    field = xrtValueObjectGet(root, XRT_STR_LITERAL("tags"));
    if (!field || xrtValueType(field) != XVALUE_ARRAY || xrtValueCount(field) != 1) goto done;
    field = xrtValueArrayGet(field, 0);
    if (!field || !xrtValueGetString(field, &text) || text.Size != 3 ||
        memcmp(text.Data, "api", 3) != 0) goto done;
    printf("first=%.*s\n", (int)text.Size, text.Data);
    field = xrtValueObjectGet(root, XRT_STR_LITERAL("标题"));
    if (!field || !xrtValueGetString(field, &text) || text.Size != 5 ||
        memcmp(text.Data, "JSON5", 5) != 0) goto done;
    printf("title=%.*s\n", (int)text.Size, text.Data);
    if (!xjson5_valid_len(config, size)) goto done;
    puts("valid=true");
    selected = xjson5_get_value_len(config, size, "workers");
    if (!selected || xjson5_has_error() || !xrtValueGetInt(selected, &integer) ||
        integer != 4) goto done;
    printf("typed=%lld\n", (long long)integer);
    path_text = xjson5_get_text_len(config, size, "service");
    if (!path_text || xjson5_has_error() || strcmp(path_text, "xlang") != 0) goto done;
    printf("path=%s\n", path_text);
    /* 大整数转 float64 只舍入一次；不要依赖显示文本判断精度。 */
    xrtValueRelease(selected);
    const char wide_hex[] = "{n:0x8000000000000401}";
    selected = xjson5_get_value_len(wide_hex, sizeof(wide_hex) - 1u, "n");
    double number;
    if (!selected || xjson5_has_error() || !xrtValueGetFloat(selected, &number) ||
        number != 9223372036854775808.0 + 2048.0) goto done;
    status = 0;
done:
    xrtFree(path_text);
    xrtValueRelease(selected);
    xrtValueRelease(root);
    return status;
}

/* 单文件范例可直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../vendor/pdjson5.c"
#include "../json5_native.c"
