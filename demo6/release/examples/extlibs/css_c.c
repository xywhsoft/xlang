#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/css/include/css_native.h"

#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    const char* sheet = "h1 { color: navy; font-size: 24px; } p { line-height: 1.5; }";
    void* color = xcss_get(sheet, "h1", "color");
    void* size = xcss_get(sheet, "h1", "font-size");
    void* selector = xcss_selector_at(sheet, 0);
    void* commented = xcss_get("a/* selector */ { color/* name */: red; }",
        "a", "color");
    int64_t rules = xcss_count_rules(sheet);
    bool valid = xcss_validate(sheet);
    int ok = color != NULL && size != NULL && selector != NULL &&
        commented != NULL && rules >= 0 && valid;

    if (ok) {
        printf("h1 color:  %s\n", (const char*)color);
        printf("h1 size:   %s\n", (const char*)size);
        printf("rules:     %" PRId64 "\n", rules);
        printf("sel[0]:    %s\n", (const char*)selector);
        puts("valid:     true");
        printf("comment:  %s\n", (const char*)commented);
        puts("css C/XL example: PASS");
    }

    xrtFree(selector);
    xrtFree(commented);
    xrtFree(size);
    xrtFree(color);
    return ok ? 0 : 2;
}

/* 单文件范例：xl run 直接编入本包实现；独立项目仍按 manifest 链接。 */
#include "../../extlibs/css/css_native.c"
