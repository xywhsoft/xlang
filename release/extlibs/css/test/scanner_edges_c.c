#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/css_native.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
static unsigned checks;

static void strip_case(const char* input, const char* expected)
{
    char* result = xcss_strip_comments(input);
    bool same = expected ? result && strcmp(result, expected) == 0 && !xrtGetError() :
        !result && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    if (!same) {
        fprintf(stderr, "CSS strip edge: [%s] expected=[%s] actual=[%s]\n",
            input, expected ? expected : "ValueError", result ? result : "NULL");
        ++failures;
    }
    xrtFree(result);
    ++checks;
}

static void escaped_comments(void)
{
    char slashes[9] = "", input[128], expected[128];
    for (unsigned n = 0; n < 8; ++n) {
        snprintf(input, sizeof(input), "a{x:%s/*gone*/;}", slashes);
        if (n % 2) strcpy(expected, input);
        else snprintf(expected, sizeof(expected), "a{x:%s ;}", slashes);
        strip_case(input, expected);

        /* 奇数反斜杠使引号成为普通转义字符，偶数时仍是真正的字符串开头。 */
        snprintf(input, sizeof(input), "a{x:%s\"/*gone*/;}", slashes);
        snprintf(expected, sizeof(expected), "a{x:%s\" ;}", slashes);
        strip_case(input, n % 2 ? expected : NULL);
        slashes[n] = '\\';
        slashes[n + 1] = '\0';
    }
    strip_case("a{x:\\'/*gone*/;}", "a{x:\\' ;}");
    strip_case("a{x:\\/*not-a-comment;}", "a{x:\\/*not-a-comment;}");
    strip_case("a{x:\"/*kept*/\";/*gone*/}", "a{x:\"/*kept*/\"; }");
    strip_case("a{x:\"escaped\\\"/*kept*/\";/*gone*/}", "a{x:\"escaped\\\"/*kept*/\"; }");
    strip_case("a{x:\\\\\"/*kept*/\";/*gone*/}", "a{x:\\\\\"/*kept*/\"; }");
    strip_case("段落{内容:\\\"/*删除*/;}", "段落{内容:\\\" ;}");
}

static void nesting_cases(void)
{
    const char* leaves[] = {"", "/*empty*/", "@font-face{font-family:demo;}", ".leaf{color:blue;}"};
    for (unsigned mode = 0; mode < 4; ++mode) {
        for (unsigned depth = 64; depth <= 65; ++depth) {
            char sheet[2048] = "a{color:red;}";
            size_t used = strlen(sheet);
            unsigned wrappers = depth - (mode == 2);
            for (unsigned i = 0; i < wrappers; ++i) {
                memcpy(sheet + used, "@media all{", 11);
                used += 11;
            }
            memcpy(sheet + used, leaves[mode], strlen(leaves[mode]));
            used += strlen(leaves[mode]);
            for (unsigned i = 0; i < wrappers; ++i) sheet[used++] = '}';
            sheet[used] = '\0';
            for (unsigned operation = 0; operation < 5; ++operation) {
                char* text = NULL;
                int64_t count = 0;
                bool valid = false;
                switch (operation) {
                case 0: valid = xcss_validate(sheet); break;
                case 1: count = xcss_count_rules(sheet); break;
                case 2: count = xcss_count_props(sheet, "a"); break;
                case 3: text = xcss_selector_at(sheet, 0); break;
                default: text = xcss_get(sheet, "a", "color"); break;
                }
                bool ok;
                if (depth == 65) {
                    ok = operation == 0 ? !valid : operation <= 2 ? count == -1 : text == NULL;
                    ok = ok && xrtErrorKind(xrtGetError()) == XERR_RANGE;
                } else {
                    ok = operation == 0 ? valid : operation <= 2 ?
                        count == (operation == 1 && mode == 3 ? 2 : 1) :
                        text && !strcmp(text, operation == 3 ? "a" : "red");
                    ok = ok && !xrtGetError();
                }
                if (!ok) {
                    fprintf(stderr, "CSS nesting edge: mode=%u depth=%u operation=%u\n", mode, depth, operation);
                    ++failures;
                }
                xrtFree(text);
                ++checks;
            }
            /* 注释剥离是迭代扫描，不应平白继承查询器的递归层数限制。 */
            char* stripped = xcss_strip_comments(sheet);
            if (!stripped || xrtGetError()) ++failures;
            xrtFree(stripped);
            ++checks;
        }
    }
}

static void strip_allocation_failures(void)
{
    const char* input = "a{x:\\\"/*gone*/;}";
    bool failed = false, recovered = false;
    for (uint64 budget = 0; budget < 32; ++budget) {
        if (!xrtMemDebugFailAfter(budget)) { ++failures; break; }
        char* result = xcss_strip_comments(input);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (result || xrtErrorKind(xrtGetError()) != XERR_MEMORY) ++failures;
            failed = true;
        } else {
            if (!result || strcmp(result, "a{x:\\\" ;}") || xrtGetError()) ++failures;
            recovered = true;
        }
        xrtFree(result);
        ++checks;
        if (recovered) break;
    }
    if (!failed || !recovered) ++failures;
}

int main(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    escaped_comments();
    nesting_cases();
    strip_allocation_failures();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) ++failures;
    printf("css scanner edges: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}

/* 独立 C 最小复现，不与 scanner_edges.xl 构成同名 companion。 */
#include "../css_native.c"
