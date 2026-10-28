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
        xrtMemDebugSnapshot(&after);
        ok &= before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes;
        if (!ok) { fprintf(stderr, "HCL expression allocation: %s budget=%u\n", expression, budget); return 0; }
        if (!triggered) return budget > 0;
    }
    return 0;
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
    char* value = xhcl_get("value = \"\\u0000\"", "value");
    ok &= value == NULL && xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    xrtFree(value);
    value = xhcl_get("value = \"1\" == \"1\"\nbroken {", "value");
    ok &= value == NULL && xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    xrtFree(value);
    xrtClearError();
    if (ok) puts("HCL string/expression boundaries: 17 cases PASS");
    return ok ? 0 : 1;
}
