#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
/* 独立 C/TCC 用例共用真实实现，只带入它实际依赖的 XRT 模块。 */
#include "../hcl_native.c"
#include <stdio.h>

static int check_value(cstr expression, cstr expected)
{
    char source[512], raw_expected[512];
    (void)snprintf(source, sizeof(source), "value = %s /* tail */\n", expression);
    (void)snprintf(raw_expected, sizeof(raw_expected), "%s /* tail */", expression);
    char* value = xhcl_get(source, "value");
    int ok = value != NULL && xrtGetError() == NULL && strcmp(value, expected) == 0;
    if (!ok) fprintf(stderr, "HCL expression changed: %s -> %s\n", expression, value ? value : "(error)");
    xrtFree(value);
    char* raw = xhcl_get_raw(source, "value");
    ok &= raw != NULL && strcmp(raw, raw_expected) == 0;
    xrtFree(raw);
    ok &= xhcl_validate(source) && xhcl_has(source, "value");
    return ok;
}

/* 对每种返回路径扫到第一次成功，失败不能变成 NUL 解码或损坏的表达式。 */
static int allocation_sweep(cstr expression, cstr expected)
{
    char source[512];
    (void)snprintf(source, sizeof(source), "value = %s /* tail */\n", expression);
    for (unsigned budget = 0; budget < 32; ++budget) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        if (!xrtMemDebugFailAfter(budget)) return 0;
        char* value = xhcl_get(source, "value");
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        int ok = triggered ? value == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            value != NULL && xrtGetError() == NULL && strcmp(value, expected) == 0;
        xrtFree(value);
        xrtClearError();
        if (triggered) {
            value = xhcl_get(source, "value");
            ok &= value != NULL && xrtGetError() == NULL && strcmp(value, expected) == 0;
            xrtFree(value);
            xrtClearError();
        }
        xrtMemDebugSnapshot(&after);
        ok &= before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount &&
            before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount;
        if (!ok) { fprintf(stderr, "HCL expression allocation: %s budget=%u\n", expression, budget); return 0; }
        if (!triggered) return budget > 0;
    }
    return 0;
}

/* 注释里的引号、转义和括号不是表达式 token；LF/CRLF 与三类嵌套边界一致。 */
static int line_comment_matrix(void)
{
    static const char* const starts[] = {"[1,", "(1 +", "{ a = 1,"};
    static const char* const ends[] = {"2]", "2)", "b = 2 }"};
    static const char* const markers[] = {"#", "//"};
    static const char* const payloads[] = {
        "\"unterminated", "'unterminated", "\"bad\\q\"", "\"\\uD800\"", "/* [ ) }"
    };
    static const char* const newlines[] = {"\n", "\r\n"};
    int ok = 1;
    for (unsigned shape = 0; shape < 3; ++shape) {
        for (unsigned marker = 0; marker < 2; ++marker) {
            for (unsigned payload = 0; payload < 5; ++payload) {
                for (unsigned newline = 0; newline < 2; ++newline) {
                    char expression[256];
                    (void)snprintf(expression, sizeof(expression), "%s %s %s%s  %s",
                        starts[shape], markers[marker], payloads[payload], newlines[newline], ends[shape]);
                    ok &= check_value(expression, expression);
                    ok &= allocation_sweep(expression, expression);
                }
            }
        }
    }
    return ok;
}

int main(void)
{
    static const struct { cstr expression; cstr value; } cases[] = {
        {"\"left\" == \"right\"", "\"left\" == \"right\""},
        {"\"left\" != \"right\"", "\"left\" != \"right\""},
        {"\"left\" + \"right\"", "\"left\" + \"right\""},
        {"\"a\" /* middle */ == \"b\"", "\"a\" /* middle */ == \"b\""},
        {"\"\\n\" == \"\\r\"", "\"\\n\" == \"\\r\""},
        {"\"\\u0061\" == \"a\"", "\"\\u0061\" == \"a\""},
        {"\"\\u0000\" == \"x\"", "\"\\u0000\" == \"x\""},
        {"\"a\" == \"b\" ? \"c\" : \"d\"", "\"a\" == \"b\" ? \"c\" : \"d\""},
        {"format(\"%s\", \"a\")", "format(\"%s\", \"a\")"},
        {"(\"a\" == \"b\")", "(\"a\" == \"b\")"},
        {"[\"a\", \"b\"]", "[\"a\", \"b\"]"},
        {"\"\"", ""}, {"\"plain\"", "plain"}, {"\"a\\\"b\"", "a\"b"},
        {"\"\\u4F60\\u597D\"", "\xe4\xbd\xa0\xe5\xa5\xbd"},
        {"\"line\\nnext\"", "line\nnext"}, {"\"/* literal */\"", "/* literal */"}
    };
    int ok = 1;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    xrtSetErrorInfo(XERR_MEMORY, "test.hcl", 0, "warm error slot");
    xrtClearError();
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ok &= check_value(cases[i].expression, cases[i].value);
        ok &= allocation_sweep(cases[i].expression, cases[i].value);
    }
    ok &= line_comment_matrix();
    char* value = xhcl_get("value = \"\\u0000\"", "value");
    ok &= value == NULL && xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    xrtFree(value);
    static const char* const invalid[] = {
        "value = \"1\" == \"1\"\nbroken {",
        "value = [1, # ignored \"\n  2]\nbroken {",
        "value = [1, // ignored \"\n  \"bad\\q\"]",
        "value = [1, # ignored \"\n  2",
        "value = [1, # \xff\n  2]"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ok &= !xhcl_validate(invalid[i]) && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        value = xhcl_get(invalid[i], "value");
        ok &= value == NULL && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        xrtFree(value);
        value = xhcl_get_raw(invalid[i], "value");
        ok &= value == NULL && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        xrtFree(value);
    }
    xrtClearError();
    if (ok) puts("HCL string/expression boundaries: 77 cases PASS");
    return ok ? 0 : 1;
}
