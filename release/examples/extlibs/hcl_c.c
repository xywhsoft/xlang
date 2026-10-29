#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/hcl/include/hcl_native.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* config = "variable \"name\" {\n  default = \"myapp\"\n  title = \"\\u4F60\\U0001F642\"\n}\n\n"
        "resource \"server\" \"main\" {\n  port/* service port */ = 8080/* default */\n  tls = true\n  ratio = 1.5\n  filter = \"prod\" == \"dev\" /* expression */\n}\n";
    char* name = xhcl_get(config, "default");
    char* title = xhcl_get(config, "title");
    char* raw = xhcl_get_raw(config, "port");
    char* label = xhcl_block_label(config, "resource", 0);
    char* second_label = xhcl_block_label_at(config, "resource", 0, 1);
    /* 不按首尾引号猜测字符串，也不把表达式当作布尔值求值。 */
    char* filter = xhcl_get(config, "filter");
    char* raw_filter = xhcl_get_raw(config, "filter");
    int ok = name != NULL && title != NULL && raw != NULL &&
        label != NULL && second_label != NULL && filter != NULL && raw_filter != NULL &&
        strcmp(name, "myapp") == 0 &&
        strcmp(title, "\xe4\xbd\xa0\xf0\x9f\x99\x82") == 0 &&
        strcmp(raw, "8080/* default */") == 0 &&
        strcmp(label, "server") == 0 &&
        strcmp(second_label, "main") == 0 &&
        strcmp(filter, "\"prod\" == \"dev\"") == 0 &&
        strcmp(raw_filter, "\"prod\" == \"dev\" /* expression */") == 0 &&
        xhcl_get_int(config, "port") == 8080 &&
        xhcl_get_float(config, "ratio") == 1.5 && xhcl_get_bool(config, "tls") &&
        xhcl_has(config, "tls") && !xhcl_has(config, "missing") &&
        xhcl_count_blocks(config, "resource") == 1 &&
        xhcl_block_label_count(config, "resource", 0) == 2 &&
        xhcl_count_attrs(config) == 6 && xhcl_validate(config);

    if (ok) {
        printf("name:    %s\n", name);
        printf("unicode:  %s\n", title);
        printf("port:    %" PRId64 "\n", xhcl_get_int(config, "port"));
        printf("tls:     %s\n", xhcl_get_bool(config, "tls") ? "true" : "false");
        printf("has tls:  %s\n", xhcl_has(config, "tls") ? "true" : "false");
        printf("has none: %s\n", xhcl_has(config, "missing") ? "true" : "false");
        printf("blocks:  %" PRId64 "\n", xhcl_count_blocks(config, "resource"));
        printf("label:   %s\n", label);
        printf("filter:  %s\n", filter);
        puts("valid:   true");
        puts("hcl C/XL example: PASS");
    }
    xrtFree(raw_filter);
    xrtFree(filter);
    xrtFree(second_label);
    xrtFree(label);
    xrtFree(raw);
    xrtFree(title);
    xrtFree(name);
    return ok ? 0 : 2;
}

/* 单文件范例：xl run 直接编入本包实现；独立项目仍按 manifest 链接。 */
#include "../../extlibs/hcl/hcl_native.c"
