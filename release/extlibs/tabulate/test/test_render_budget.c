#define XRT_MODULE_MEMORY_DEBUG
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_STRING_FORMAT
#include "../tabulate_native.c"
#ifndef XTAB_TEST_RENDER
#define XTAB_TEST_RENDER "../tabulate_render.c"
#endif
#include XTAB_TEST_RENDER
#include <stdio.h>

static unsigned failures, layouts, allocationFaults;

static void check(bool okay, cstr message)
{
    if (!okay) {
        if (failures < 12u) fprintf(stderr, "render budget: %s\n", message);
        ++failures;
    }
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

/* 同一表重复渲染，预算0须失败，预算1须成功；失败不改变表和已返回的文本。 */
static void render_layout(void* table, cstr corpus, unsigned number, unsigned phase)
{
    char* expected = xtab_render(table);
    check(expected != NULL && xrtGetError() == NULL, "baseline render");
    if (expected == NULL) return;
    ++layouts;
    for (unsigned budget = 0u; budget < 2u; ++budget) {
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "inject allocation failure");
        char* actual = xtab_render(table);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (budget == 0u) {
            check(actual == NULL && triggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "first output allocation must fail");
            if (triggered) ++allocationFaults;
        } else {
            check(actual != NULL && !triggered && xrtGetError() == NULL && strcmp(actual, expected) == 0,
                "render needs more than one output allocation");
        }
        xrtFree(actual);
        check(balanced(&before), "render memory balance");
    }
    if (corpus != NULL) {
        str path = xrtFormat("%s/case-%u-%u.txt", corpus, number, phase);
        check(path != NULL && xrtFileWriteAll(path, (xbytesview){(cbytes)expected, strlen(expected)}),
            "write independent layout corpus");
        xrtFree(path);
    }
    xrtFree(expected);
}

static void layout_matrix(cstr corpus)
{
    static const char* words[] = {"", "abc", "\xe6\xb1\x89\xe5\xad\x97", "e\xcc\x81",
        "\xf3\xa0\x84\x80", "a,b"};
    static const unsigned columns[] = {1, 2, 4, 32}, rows[] = {0, 1, 7, 16};
    const char* cells[32];
    unsigned number = 0u;
    for (unsigned c = 0u; c < 4u; ++c) {
        for (unsigned h = 0u; h < 3u; ++h) {
            for (unsigned r = 0u; r < 4u; ++r, ++number) {
                void* table = xtab_create();
                check(table != NULL, "create matrix table");
                if (table == NULL) return;
                unsigned count = h == 0u ? 0u : h == 1u ? 1u : columns[c];
                for (unsigned i = 0u; i < count; ++i) cells[i] = words[(i + h) % 6u];
                check(xtab_set_headers(table, cells, count), "matrix headers");
                for (unsigned i = 0u; i < rows[r]; ++i) {
                    unsigned size = i % columns[c] + 1u;
                    for (unsigned j = 0u; j < size; ++j) cells[j] = words[(i * 3u + j) % 6u];
                    check(xtab_add_row(table, cells, size), "matrix data row");
                }
                render_layout(table, corpus, number, 0u);
                check(xtab_set_headers(table, NULL, 0u), "clear headers");
                render_layout(table, corpus, number, 1u);
                for (unsigned i = 0u; i < columns[c]; ++i) cells[i] = words[(i + 4u) % 6u];
                check(xtab_set_headers(table, cells, columns[c]), "replace headers");
                render_layout(table, corpus, number, 2u);
                xtab_destroy(table);
            }
        }
    }
}

/* 260条等宽行+UTF-8额外字节，构造含终止符恰好4MiB与多1字节的真实表。 */
static void* limit_table(unsigned extra)
{
    char wide[4097], last[3830], chinese[3 * 244 + 1];
    const char* headers[] = {wide, wide, wide, last};
    const char* cell = chinese;
    memset(wide, 'A', sizeof(wide) - 1u); wide[sizeof(wide) - 1u] = 0;
    memset(last, 'B', sizeof(last) - 1u); last[sizeof(last) - 1u] = 0;
    for (unsigned i = 0u; i < extra; ++i) memcpy(chinese + 3u * i, "\xe4\xb8\xad", 3u);
    chinese[3u * extra] = 0;
    void* table = xtab_create();
    check(table != NULL && xtab_set_headers(table, headers, 4u) && xtab_add_row(table, &cell, 1u),
        "limit table setup");
    cell = "x";
    for (unsigned i = 1u; i < 256u; ++i) check(xtab_add_row(table, &cell, 1u), "limit row setup");
    return table;
}

static void output_limit(void)
{
    void* table = limit_table(243u);
    char* text = xtab_render(table);
    check(text != NULL && strlen(text) + 1u == XTAB_MAX_OUTPUT, "exact output limit");
    xrtFree(text);
    render_layout(table, NULL, 0u, 0u);
    xtab_destroy(table);
    table = limit_table(244u);
    for (unsigned inject = 0u; inject < 3u; ++inject) {
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        if (inject) check(xrtMemDebugFailAfter(inject == 1u ? 0u : 4u), "limit fault setup");
        text = xtab_render(table);
        bool triggered = inject && xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        check(text == NULL && (inject == 1u ? triggered : !triggered) &&
            xrtErrorKind(xrtGetError()) == (inject == 1u ? XERR_MEMORY : XERR_RANGE),
            "oversized render allocated output or lost Range/diagnostic OOM distinction");
        xrtFree(text);
        check(balanced(&before), "limit memory balance");
        check(xtab_row_count(table) == 256 && xtab_col_count(table) == 4, "limit changes table");
    }
    /* 去掉宽表头后可立刻重用，不遗留部分输出或错误状态。 */
    check(xtab_set_headers(table, NULL, 0u), "limit recovery");
    text = xtab_render(table);
    check(text != NULL && xrtGetError() == NULL, "limit recovery render");
    xrtFree(text);
    xtab_destroy(table);
}

int main(int argc, char** argv)
{
    if (!xrtMemDebugEnabled()) return 1;
    xrtSetErrorInfo(XERR_MEMORY, "test.tabulate", 0, "warm diagnostic slot");
    layout_matrix(argc > 1 ? argv[1] : NULL);
    output_limit();
    printf("tabulate render budget: %u layouts, %u output faults, %u failures\n",
        layouts, allocationFaults, failures);
    return failures == 0u ? 0 : 1;
}
