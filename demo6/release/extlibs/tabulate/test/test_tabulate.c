#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/tabulate_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "tabulate C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int test_error_state(void)
{
    const char* invalid[] = {"bad\ncell"};
    const char* valid[] = {"ok"};
    void* table;
    char* rendered;

    CHECK(!xtab_begin_row(NULL) && xrtGetError() != NULL);
    table = xtab_create();
    CHECK(table != NULL && xrtGetError() == NULL);
    CHECK(!xtab_set_headers(table, invalid, 1) && xrtGetError() != NULL);
    CHECK(xtab_row_count(table) == 0 && xrtGetError() == NULL);
    CHECK(!xtab_add_row(table, NULL, 1) && xrtGetError() != NULL);
    CHECK(xtab_add_row(table, valid, 1) && xrtGetError() == NULL);
    CHECK(!xtab_begin_row(NULL) && xrtGetError() != NULL);
    rendered = (char*)xtab_render(table);
    CHECK(rendered != NULL && xrtGetError() == NULL);
    xrtFree(rendered);
    CHECK(!xtab_begin_row(NULL) && xrtGetError() != NULL);
    CHECK(xtab_begin_row(table) && xrtGetError() == NULL);
    CHECK(xtab_append_cell(table, "next", 4) && xtab_commit(table));
    CHECK(xtab_begin_row(table));
    CHECK(!xtab_append_cell(table, "bad\n", 4) && xrtGetError() != NULL);
    xtab_abort(table);
    CHECK(xrtGetError() != NULL);
    CHECK(!xtab_add_row(table, NULL, 1) && xrtGetError() != NULL);
    CHECK(xtab_col_count(table) == 1 && xrtGetError() == NULL);
    xtab_destroy(table);
    return 0;
}

static int test_transaction_faults(void)
{
    const char* headers[] = {"old"};
    const char* replacement[] = {"new"};
    void* table = xtab_create();
    char* rendered;
    CHECK(table != NULL && xtab_set_headers(table, headers, 1));

    CHECK(xtab_begin_row(table));
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(!xtab_append_cell(table, "cell", 4));
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xtab_abort(table);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    CHECK(xtab_row_count(table) == 0 && xrtGetError() == NULL);

    CHECK(xtab_begin_row(table) && xtab_append_cell(table, "cell", 4));
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(!xtab_commit(table));
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    CHECK(xtab_row_count(table) == 0);
    CHECK(xtab_begin_row(table) && xtab_append_cell(table, "cell", 4) &&
        xtab_commit(table));

    CHECK(xrtMemDebugFailAfter(0));
    CHECK(!xtab_set_headers(table, replacement, 1));
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    CHECK(xtab_row_count(table) == 1);

    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xtab_render(table) == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    rendered = (char*)xtab_render(table);
    CHECK(rendered != NULL && xrtGetError() == NULL &&
        strstr(rendered, "old") != NULL && strstr(rendered, "cell") != NULL &&
        strstr(rendered, "new") == NULL);
    xrtFree(rendered);
    xtab_destroy(table);
    return 0;
}

static int test_format_controls(void)
{
    const char* controls[] = {
        "a\xC2\xAD" "b", /* U+00AD soft hyphen */
        "a\xE2\x80\x8B" "b", /* U+200B zero-width space */
        "a\xE2\x80\xAE" "b", /* U+202E right-to-left override */
        "a\xE2\x81\xA6" "b", /* U+2066 left-to-right isolate */
        "a\xE2\x80\xA8" "b", /* U+2028 line separator */
        "a\xE2\x80\xA9" "b"  /* U+2029 paragraph separator */
    };
    const char* valid[] = {"plain"};
    void* table = xtab_create();
    size_t i;
    CHECK(table != NULL);
    for (i = 0u; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        CHECK(!xtab_add_row(table, &controls[i], 1u));
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        CHECK(xtab_row_count(table) == 0);
    }
    CHECK(xtab_add_row(table, valid, 1u) && xtab_row_count(table) == 1);
    xtab_destroy(table);
    return 0;
}

static int test_supplementary_variation(void)
{
    const char* cells[] = {"文\xF3\xA0\x84\x80", "x"}; /* U+E0100 不占列。 */
    void* table = xtab_create();
    char* rendered;
    CHECK(table != NULL && xtab_add_row(table, cells, 2));
    rendered = (char*)xtab_render(table);
    CHECK(rendered != NULL && strcmp(rendered,
        "+----+---+\n| 文\xF3\xA0\x84\x80 | x |\n+----+---+\n") == 0);
    xrtFree(rendered);
    xtab_destroy(table);
    return 0;
}

static int test_render_allocations(void)
{
    const char* row[] = {"alpha", "\xE4\xBD\xA0\xE5\xA5\xBD", "forty-two"};
    void* table = xtab_create();
    char* expected;
    unsigned point, failures = 0u, successes = 0u;
    int index;
    CHECK(table != NULL);
    for (index = 0; index < 64; ++index)
        CHECK(xtab_add_row(table, row, 3));
    expected = (char*)xtab_render(table);
    CHECK(expected != NULL);
    for (point = 0u; point < 64u; ++point) {
        char* rendered;
        int triggered;
        CHECK(xrtMemDebugFailAfter((uint64)point));
        rendered = (char*)xtab_render(table);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (rendered == NULL) {
            CHECK(triggered && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            CHECK(strcmp(rendered, expected) == 0 && xrtGetError() == NULL);
            ++successes;
        }
        xrtFree(rendered);
        CHECK(xtab_row_count(table) == 64);
    }
    CHECK(failures != 0u && successes != 0u);
    xrtFree(expected);
    xtab_destroy(table);
    return 0;
}

int main(void)
{
    const char* headers[] = {"名", "Age"};
    const char* row[] = {"你好", "3"};
    const char* comma[] = {"", "a,b"};
    const char* shorter[] = {"X"};
    const char* one[] = {"x"};
    const char* invalid_header[] = {"bad\nheader"};
    const char* null_cell[] = {"ok", NULL};
    char long_cell[4097];
    char unterminated_cell[4097];
    char budget_cell[1025];
    const char* long_row[] = {long_cell};
    const char* four_budget[] = {budget_cell, budget_cell, budget_cell, budget_cell};
    const char* wide[33];
    void* table;
    void* secondary;
    char* first;
    char* second;
    int i;
    CHECK(test_error_state() == 0);
    CHECK(test_transaction_faults() == 0);
    CHECK(test_format_controls() == 0);
    CHECK(test_supplementary_variation() == 0);
    CHECK(test_render_allocations() == 0);
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xtab_create() == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    table = xtab_create();
    secondary = xtab_create();
    CHECK(table != NULL && secondary != NULL);
    CHECK(xtab_row_count(table) == 0 && xtab_col_count(table) == 0);
    first = (char*)xtab_render(table);
    CHECK(first != NULL && strcmp(first, "") == 0);
    xrtFree(first);

    CHECK(xtab_set_headers(table, headers, 2));
    CHECK(xtab_add_row(table, row, 2));
    CHECK(xtab_add_row(table, comma, 2));
    CHECK(xtab_row_count(table) == 2 && xtab_col_count(table) == 2);
    first = (char*)xtab_render(table);
    CHECK(first != NULL);
    CHECK(strcmp(first,
        "+------+-----+\n"
        "| 名   | Age |\n"
        "+------+-----+\n"
        "| 你好 | 3   |\n"
        "|      | a,b |\n"
        "+------+-----+\n") == 0);
    second = (char*)xtab_render(table);
    CHECK(second != NULL && second != first && strcmp(first, second) == 0);
    xrtFree(second);
    xrtFree(first);

    /* 替换表头、失败回滚都不能破坏已提交行。 */
    CHECK(xtab_set_headers(table, shorter, 1));
    CHECK(xtab_col_count(table) == 2);
    CHECK(!xtab_set_headers(table, invalid_header, 1));
    CHECK(xtab_col_count(table) == 2 && xtab_row_count(table) == 2);
    CHECK(xtab_begin_row(table));
    CHECK(xtab_append_cell(table, "ok", 2));
    CHECK(!xtab_append_cell(table, "bad\n", 4));
    xtab_abort(table);
    CHECK(xtab_row_count(table) == 2);
    CHECK(xtab_set_headers(table, NULL, 0));
    CHECK(xtab_col_count(table) == 2);

    memset(long_cell, 'A', 4096u);
    long_cell[4096] = '\0';
    memset(unterminated_cell, 'B', sizeof(unterminated_cell));
    {
        const char* unterminated_row[] = {unterminated_cell};
        CHECK(!xtab_add_row(secondary, unterminated_row, 1));
        CHECK(xrtGetError() != NULL);
        CHECK(xtab_row_count(secondary) == 0);
    }
    memset(budget_cell, 'B', 1024u);
    budget_cell[1024] = '\0';
    CHECK(xtab_add_row(secondary, long_row, 1));
    first = (char*)xtab_render(secondary);
    CHECK(first != NULL && strlen(first) > 4096u && strstr(first, long_cell) != NULL);
    xrtFree(first);
    CHECK(!xtab_append_cell(secondary, "a", 1));
    CHECK(xtab_begin_row(secondary));
    CHECK(!xtab_append_cell(secondary, long_cell, 4097));
    xtab_abort(secondary);
    CHECK(xtab_row_count(secondary) == 1);
    CHECK(xtab_begin_row(secondary));
    CHECK(!xtab_append_cell(secondary, "a\0b", 3));
    xtab_abort(secondary);
    CHECK(xtab_begin_row(secondary));
    CHECK(!xtab_append_cell(secondary, "\xc0\xaf", 2));
    xtab_abort(secondary);
    CHECK(xtab_begin_row(secondary));
    CHECK(!xtab_append_cell(secondary, "\x1b[31m", 5));
    xtab_abort(secondary);
    CHECK(!xtab_add_row(secondary, NULL, 1));
    xrtClearError();
    CHECK(!xtab_add_row(secondary, null_cell, 2));
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xtab_row_count(secondary) == 1);
    xrtClearError();
    CHECK(!xtab_set_headers(secondary, null_cell, 2));
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xtab_col_count(secondary) == 1);
    CHECK(xtab_add_row(secondary, one, 1));
    CHECK(xtab_row_count(secondary) == 2);
    for (i = 0; i < 33; ++i) wide[i] = "x";
    CHECK(!xtab_add_row(secondary, wide, 33));

    xtab_destroy(secondary);
    secondary = xtab_create();
    CHECK(secondary != NULL);
    for (i = 0; i < 256; ++i) CHECK(xtab_add_row(secondary, one, 1));
    CHECK(!xtab_add_row(secondary, one, 1));
    CHECK(xtab_row_count(secondary) == 256);
    xtab_destroy(secondary);

    secondary = xtab_create();
    CHECK(secondary != NULL && xtab_set_headers(secondary, one, 1));
    for (i = 0; i < 255; ++i) CHECK(xtab_add_row(secondary, four_budget, 4));
    CHECK(!xtab_add_row(secondary, four_budget, 4));
    CHECK(xtab_row_count(secondary) == 255);
    first = (char*)xtab_render(secondary);
    CHECK(first != NULL && strstr(first, budget_cell) != NULL);
    xrtFree(first);
    xtab_destroy(secondary);
    xtab_destroy(table);
    puts("tabulate C boundaries: PASS");
    return 0;
}
