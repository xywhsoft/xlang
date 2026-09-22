/* CSS C smoke: same syntax-aware restricted scanner as the xlang package. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

cstr xcss_get(cstr sCss, cstr sSelector, cstr sProperty);
long long xcss_count_rules(cstr sCss);
long long xcss_count_props(cstr sCss, cstr sSelector);
cstr xcss_selector_at(cstr sCss, long long iIndex);
bool xcss_validate(cstr sCss);
cstr xcss_strip_comments(cstr sCss);

static int fail(cstr sMessage)
{
    fprintf(stderr, "css C smoke failed: %s\n", sMessage);
    return 1;
}

int main(void)
{
    cstr sCss = "/* header */ @font-face { font-family: 'demo'; } "
        "@media screen { .card, .panel { content: \"{;}\"; color: rgb(1, 2, 3); } }";

    if (!xcss_validate(sCss) || xcss_count_rules(sCss) != 1 ||
        xcss_count_props(sCss, ".panel") != 2 ||
        strcmp(xcss_selector_at(sCss, 0), ".card, .panel") != 0 ||
        strcmp(xcss_get(sCss, ".card", "content"), "\"{;}\"") != 0 ||
        strcmp(xcss_get(sCss, ".panel", "color"), "rgb(1, 2, 3)") != 0 ||
        xcss_validate("a { color: red; /* missing") ||
        strstr(xcss_strip_comments("a/*x*/b"), "/*") != NULL) {
        return fail("syntax aware scanner");
    }
    puts("css C smoke: PASS");
    return 0;
}
