#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/css/include/css_native.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* sheet = "h1 { color: navy; font-size: 24px; } p { line-height: 1.5; }";
    void* color = xcss_get(sheet, "h1", "color");
    void* size = xcss_get(sheet, "h1", "font-size");
    void* selector = xcss_selector_at(sheet, 0);
    void* clean = xcss_strip_comments("h1/* note */{ color: navy; }");
    void* commented = xcss_get("a/* selector */ { color/* name */: red; }",
        "a", "color");
    int64_t rules = xcss_count_rules(sheet);
    int64_t props = xcss_count_props(sheet, "h1");
    bool valid = xcss_validate(sheet);
    int ok = color != NULL && size != NULL && selector != NULL &&
        clean != NULL && commented != NULL && rules == 2 && props == 2 && valid &&
        strcmp((const char*)color, "navy") == 0 &&
        strcmp((const char*)size, "24px") == 0 &&
        strcmp((const char*)selector, "h1") == 0 &&
        strcmp((const char*)clean, "h1 { color: navy; }") == 0 &&
        strcmp((const char*)commented, "red") == 0;

    if (ok) {
        printf("h1 color:  %s\n", (const char*)color);
        printf("h1 size:   %s\n", (const char*)size);
        printf("rules:     %" PRId64 "\n", rules);
        printf("props:     %" PRId64 "\n", props);
        printf("sel[0]:    %s\n", (const char*)selector);
        puts("valid:     true");
        printf("clean:     %s\n", (const char*)clean);
        printf("comment:  %s\n", (const char*)commented);
        puts("css C/XL example: PASS");
    }

    xrtFree(selector);
    xrtFree(clean);
    xrtFree(commented);
    xrtFree(size);
    xrtFree(color);
    return ok ? 0 : 2;
}

/* 单文件范例：xl run 直接编入本包实现；独立项目仍按 manifest 链接。 */
#include "../../extlibs/css/css_native.c"
