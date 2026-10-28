/* 单文件 C 对照，直接复用包实现；不与 audit.xl 构成同名 companion。 */
#include "../css_native.c"
#include <inttypes.h>
#include <stdio.h>

static bool print_property(const char* label, const char* sheet, const char* name)
{
    char* value = xcss_get(sheet, ".badge", name);
    if (!value) return false;
    printf("%s: %s\n", label, value);
    xrtFree(value);
    return true;
}

int main(void)
{
    const char* source = "/* theme */ .badge { --quote: \\\"/* removed */; --slash: \\/*literal; content: \"/* kept */\"; color: #3366CC; } @media print { .badge { color: black; } }";
    char* clean = xcss_strip_comments(source);
    if (!clean) return 1;
    char* selector = xcss_selector_at(clean, 0);
    int64_t rules = xcss_count_rules(clean);
    int64_t properties = xcss_count_props(clean, ".badge");
    bool ok = selector && rules == 2 && properties == 4 &&
        xcss_validate(source) && xcss_validate(clean);
    if (ok) {
        printf("rules: %" PRId64 "\nproperties: %" PRId64 "\nselector: %s\n", rules, properties, selector);
        ok = print_property("quote", clean, "--quote") && print_property("slash", clean, "--slash") &&
            print_property("content", clean, "content") && print_property("color", clean, "color");
    }
    xrtFree(selector);
    xrtFree(clean);
    return ok && !ferror(stdout) ? 0 : 1;
}
