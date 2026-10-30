#define XRT_MODULE_ALL
#include "xrt.h"
#include "../md4c_native.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    const char* source = argc > 1 ? argv[1] : "release/extlibs/md4c/examples/article.md";
    int parser = xmd4c_github_flags() | xmd4c_no_html_flag();
    int renderer = xmd4c_skip_utf8_bom_flag() | xmd4c_xhtml_flag();
    char* html = xmd4c_file_html(source, parser, renderer);
    if (!html) { fprintf(stderr, "%s\n", xmd4c_last_error()); return 1; }
    /* 直接使用并释放 XRT 返回缓冲，不转为中间容器或复制正文。 */
    int failed = fputs(html, stdout) == EOF;
    xrtFree(html);
    return failed;
}

#include "../third_party/md4c/md4c.c"
#undef ISDIGIT
#undef ISLOWER
#undef ISUPPER
#undef ISALNUM
#include "../third_party/md4c/md4c-html.c"
#include "../third_party/md4c/entity.c"
#include "../md4c_native.c"
