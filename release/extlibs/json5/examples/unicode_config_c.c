/* 与 XLang 同一份 UTF-8 配置，三个值都独立断言，不只比较输出。 */
#include "../json5_native.c"
#include "../vendor/pdjson5.c"
#include <stdio.h>

int main(void)
{
    const char source[] = "{title:'hello\\\xe2\x80\xa8世界', label:'\\你', // 配置\xe2\x80\xa9port:8080}";
    xvalue* root = xjson5_parse_len(source, sizeof(source) - 1);
    xvalue* label = NULL;
    str title = NULL;
    xstrview view;
    int64 port;
    int ok = 0;
    if (!root || xrtGetError() || !xjson5_valid_len(source, sizeof(source) - 1)) goto done;
    title = xjson5_get_text_len(source, sizeof(source) - 1, "title");
    label = xjson5_get_value_len(source, sizeof(source) - 1, "label");
    if (!title || strcmp(title, "hello世界") || !label || !xrtValueGetString(label, &view) ||
        view.Size != strlen("你") || memcmp(view.Data, "你", view.Size) ||
        !xrtValueGetInt(xrtValueObjectGet(root, XRT_STR_LITERAL("port")), &port) || port != 8080) goto done;
    puts("title=hello世界\nlabel=你\nport=8080\nJSON5 unicode config: OK");
    ok = 1;
done:
    xrtFree(title);
    xrtValueRelease(label);
    xrtValueRelease(root);
    xrtClearError();
    return ok ? 0 : 1;
}
