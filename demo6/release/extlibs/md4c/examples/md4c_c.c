#define XRT_MODULE_ALL
#include "xrt.h"
#include "../md4c_native.h"
#include <stdio.h>

int main(void) {
    char *html = xmd4c_html("# Title\n\nHello **xlang**.");
    char *safe = xmd4c_html_flags("safe <script>bad</script>",
        xmd4c_github_flags() | xmd4c_no_html_flag(), 0);
    if (!html || !safe) {
        xrtFree(html);
        xrtFree(safe);
        return 1;
    }
    fputs(html, stdout);
    fputs(safe, stdout);
    xrtFree(html);
    xrtFree(safe);
    return 0;
}

/* 单文件范例可直接运行；正式包仍分别编译这些源文件。 */
#include "../third_party/md4c/md4c.c"
/* 两个上游编译单元各有一套字符分类宏，合并时清除第一套。 */
#undef ISDIGIT
#undef ISLOWER
#undef ISUPPER
#undef ISALNUM
#include "../third_party/md4c/md4c-html.c"
#include "../third_party/md4c/entity.c"
#include "../md4c_native.c"
