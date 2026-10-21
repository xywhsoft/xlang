#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/hcl/include/hcl_native.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* config = "variable \"name\" {\n  default = \"myapp\"\n  title = \"\\u4F60\\U0001F642\"\n}\n\n"
        "resource \"server\" \"main\" {\n  port/* service port */ = 8080/* default */\n  tls = true\n}\n";
    void* name = xhcl_get(config, "default");
    void* title = xhcl_get(config, "title");
    void* label = xhcl_block_label(config, "resource", 0);
    int ok = name != NULL && title != NULL && label != NULL &&
        strcmp((const char*)name, "myapp") == 0 &&
        strcmp((const char*)title, "\xe4\xbd\xa0\xf0\x9f\x99\x82") == 0 &&
        strcmp((const char*)label, "server") == 0 &&
        xhcl_get_int(config, "port") == 8080 && xhcl_get_bool(config, "tls") &&
        xhcl_has(config, "tls") && !xhcl_has(config, "missing") &&
        xhcl_count_blocks(config, "resource") == 1 && xhcl_validate(config);

    if (ok) {
        printf("name:    %s\n", (const char*)name);
        printf("unicode:  %s\n", (const char*)title);
        printf("port:    %" PRId64 "\n", xhcl_get_int(config, "port"));
        printf("tls:     %s\n", xhcl_get_bool(config, "tls") ? "true" : "false");
        printf("has tls:  %s\n", xhcl_has(config, "tls") ? "true" : "false");
        printf("has none: %s\n", xhcl_has(config, "missing") ? "true" : "false");
        printf("blocks:  %" PRId64 "\n", xhcl_count_blocks(config, "resource"));
        printf("label:   %s\n", (const char*)label);
        puts("valid:   true");
        puts("hcl C/XL example: PASS");
    }
    xrtFree(label);
    xrtFree(title);
    xrtFree(name);
    return ok ? 0 : 2;
}

/* 单文件范例：xl run 直接编入本包实现；独立项目仍按 manifest 链接。 */
#include "../../extlibs/hcl/hcl_native.c"
