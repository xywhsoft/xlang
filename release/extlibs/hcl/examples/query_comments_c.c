/* C 与 XLang 对照：只浏览配置，表达式与内部注释保持原文，不进行求值。 */
#include "../hcl_native.c"
#include <stdio.h>

int main(void)
{
    const char* hosts = "[\n  \"api.example\", // note: \"retired\n  \"admin.example\"\n]";
    const char* config = "resource \"service\" \"web\" {\nport = 8080\nhosts = "
        "[\n  \"api.example\", // note: \"retired\n  \"admin.example\"\n] /* tail */\n}\n";
    char* value = xhcl_get(config, "hosts");
    char* raw = xhcl_get_raw(config, "hosts");
    char* label = xhcl_block_label_at(config, "resource", 0, 1);
    int ok = value && raw && label && strcmp(value, hosts) == 0 &&
        strcmp(raw, "[\n  \"api.example\", // note: \"retired\n  \"admin.example\"\n] /* tail */") == 0 &&
        strcmp(label, "web") == 0 && xhcl_validate(config) &&
        xhcl_count_attrs(config) == 2 && xhcl_get_int(config, "port") == 8080;
    if (ok) {
        puts("port: 8080");
        puts(value);
        puts("HCL query comments: OK");
    }
    xrtFree(value);
    xrtFree(raw);
    xrtFree(label);
    return ok ? 0 : 1;
}
