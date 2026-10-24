#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/slugify_native.h"
#include <stdio.h>
#include <string.h>

static int show(const char* title)
{
    char* slug = (char*)xsl_slugify(title, (long long)strlen(title));
    if (slug == NULL) return 0;
    puts(slug);
    xrtFree(slug);
    return 1;
}

int main(void)
{
    if (!show("Café & 你好") || !show("Ẁorld") || !show("Ｆｕｌｌ１２３") ||
        !show("Release v2.0 -- 新特性")) return 1;
    puts(xsl_is_ascii("hello-world", 11) == 1 ? "ASCII: true" : "ASCII: false");
    return 0;
}

/* 单文件运行由 TCC 编译实现；普通 C 构建仍单独链接包源码。 */
#ifdef __TINYC__
#include "../slugify_native.c"
#endif
