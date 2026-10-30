#define XRT_MODULE_ALL
#include "xrt.h"
#include "../md4c_native.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void rendered(const char* markdown, int parser, int renderer, const char* expected)
{
    char* html = xmd4c_html_flags(markdown, parser, renderer);
    if (!html || strcmp(html, expected) || xrtGetError()) {
        fprintf(stderr, "md4c option mismatch: %s\nexpected: %s\nactual: %s\n",
            markdown, expected, html ? html : "(null)");
        ++failures;
    }
    xrtFree(html);
}

int main(void)
{
    int common = xmd4c_commonmark_flags(), github = xmd4c_github_flags();
    rendered("\xef\xbb\xbf# Title", common, xmd4c_skip_utf8_bom_flag(), "<h1>Title</h1>\n");
    rendered("---\n\na  \nb\n\n![x](/p)", common, 0,
        "<hr>\n<p>a<br>\nb</p>\n<p><img src=\"/p\" alt=\"x\"></p>\n");
    rendered("---\n\na  \nb\n\n![x](/p)", common, xmd4c_xhtml_flag(),
        "<hr />\n<p>a<br />\nb</p>\n<p><img src=\"/p\" alt=\"x\" /></p>\n");
    rendered("&copy; &#x4e2d;", common, 0, "<p>\xc2\xa9 \xe4\xb8\xad</p>\n");
    rendered("&copy; &#x4e2d;", common, xmd4c_verbatim_entities_flag(),
        "<p>&copy; &#x4e2d;</p>\n");
    rendered("~~old~~", common, 0, "<p>~~old~~</p>\n");
    rendered("~~old~~", github, 0, "<p><del>old</del></p>\n");
    rendered("<b>x</b>", common, 0, "<p><b>x</b></p>\n");
    rendered("<b>x</b>", common | xmd4c_no_html_flag(), 0,
        "<p>&lt;b&gt;x&lt;/b&gt;</p>\n");
    rendered("", github, xmd4c_skip_utf8_bom_flag() | xmd4c_xhtml_flag() |
        xmd4c_verbatim_entities_flag(), "");
    rendered(NULL, common, 0, "");
    if (!xmd4c_available() || strcmp(xmd4c_version(), "md4c 0.5.2") || *xmd4c_last_error()) ++failures;
    printf("md4c render options C: 11 cases, %d failures\n", failures);
    return failures ? 1 : 0;
}

/* 独立 C 范例/测试不用同名 .xl，避免被当作其自动 companion。 */
#include "../third_party/md4c/md4c.c"
#undef ISDIGIT
#undef ISLOWER
#undef ISUPPER
#undef ISALNUM
#include "../third_party/md4c/md4c-html.c"
#include "../third_party/md4c/entity.c"
#include "../md4c_native.c"
