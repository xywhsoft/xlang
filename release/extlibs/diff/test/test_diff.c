#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/diff_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "diff C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* owned, const char* expected)
{
    int result = owned != NULL && strcmp((char*)owned, expected) == 0;
    xrtFree(owned);
    return result;
}

static int test_allocation_failure(void)
{
    unsigned failure;
    unsigned rejected;
    int kind;
    for (kind = 0; kind < 2; ++kind) {
        rejected = 0u;
        for (failure = 0u; failure < 16u; ++failure) {
            void* result;
            int triggered;
            CHECK(xrtMemDebugFailAfter(failure));
            result = kind == 0 ? xdiff_lines("old", "new") :
                xdiff_stats("old", "new");
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(result == NULL);
                CHECK(xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                xrtClearError();
                ++rejected;
            } else {
                CHECK(result != NULL);
                xrtFree(result);
                break;
            }
            CHECK(same(xdiff_lines("old", "new"), "-old\n+new\n"));
        }
        CHECK(rejected >= 4u && failure < 16u);
    }
    return 0;
}

/* 与语言回归相同的输入，完整计划和结果按当前原生分配预算验收。 */
static int test_owned_result_budget(void)
{
    const char* expected[] = {"", "0,0,0", "-old\n+new\n", "1,1,0"};
    unsigned operation, attempt;
    for (operation = 0; operation < 4u; ++operation) {
        const char* left = operation < 2u ? "" : "old";
        const char* right = operation < 2u ? "" : "new";
        unsigned needed = operation < 2u ? 3u : 5u;
        char* warm = operation % 2u == 0u ? xdiff_lines(left, right) : xdiff_stats(left, right);
        CHECK(same(warm, expected[operation]));
        for (attempt = 0; attempt < 2u; ++attempt) {
            unsigned budget = attempt == 0u ? 0u : needed;
            char* result;
            bool triggered;
            bool memory_error;
            CHECK(xrtMemDebugFailAfter(budget));
            result = operation % 2u == 0u ? xdiff_lines(left, right) : xdiff_stats(left, right);
            triggered = xrtMemDebugFailTriggered();
            memory_error = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            if (budget == 0u) {
                CHECK(result == NULL && triggered && memory_error);
            } else {
                CHECK(!triggered && !memory_error);
                CHECK(same(result, expected[operation]) && xrtGetError() == NULL);
            }
        }
    }
    return 0;
}

static unsigned next_random(unsigned* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int test_lcs_properties(void)
{
    unsigned state = 0x5eedu;
    unsigned case_index;
    for (case_index = 0u; case_index < 512u; ++case_index) {
        char old_chars[8], new_chars[8], old_text[16], new_text[16];
        char rebuilt_old[8], rebuilt_new[8], extra;
        char *output, *stats;
        size_t old_count = next_random(&state) % 8u;
        size_t new_count = next_random(&state) % 8u;
        size_t i, j, size, old_used = 0u, new_used = 0u;
        unsigned added = 0u, deleted = 0u, unchanged = 0u, best = 0u;
        unsigned stat_added, stat_deleted, stat_unchanged, mask;
        for (i = 0u; i < old_count; ++i) old_chars[i] = (char)('a' + next_random(&state) % 3u);
        for (i = 0u; i < new_count; ++i) new_chars[i] = (char)('a' + next_random(&state) % 3u);
        for (i = 0u; i < old_count; ++i) {
            old_text[i * 2u] = old_chars[i];
            old_text[i * 2u + 1u] = i + 1u == old_count ? '\0' : '\n';
        }
        for (i = 0u; i < new_count; ++i) {
            new_text[i * 2u] = new_chars[i];
            new_text[i * 2u + 1u] = i + 1u == new_count ? '\0' : '\n';
        }
        if (old_count == 0u) old_text[0] = '\0';
        if (new_count == 0u) new_text[0] = '\0';
        output = (char*)xdiff_lines(old_text, new_text);
        stats = (char*)xdiff_stats(old_text, new_text);
        CHECK(output != NULL && stats != NULL);
        size = strlen(output);
        CHECK(size % 3u == 0u);
        for (i = 0u; i < size; i += 3u) {
            CHECK(output[i + 2u] == '\n');
            if (output[i] == ' ' || output[i] == '-') rebuilt_old[old_used++] = output[i + 1u];
            if (output[i] == ' ' || output[i] == '+') rebuilt_new[new_used++] = output[i + 1u];
            if (output[i] == ' ') ++unchanged;
            else if (output[i] == '+') ++added;
            else { CHECK(output[i] == '-'); ++deleted; }
        }
        CHECK(old_used == old_count && new_used == new_count);
        CHECK(memcmp(rebuilt_old, old_chars, old_count) == 0);
        CHECK(memcmp(rebuilt_new, new_chars, new_count) == 0);
        CHECK(sscanf(stats, "%u,%u,%u%c", &stat_added, &stat_deleted,
            &stat_unchanged, &extra) == 3);
        CHECK(added == stat_added && deleted == stat_deleted && unchanged == stat_unchanged);

        /* 小序列穷举子序列，独立核对编辑结果是否为最短。 */
        for (mask = 0u; mask < (1u << old_count); ++mask) {
            unsigned matched = 0u;
            j = 0u;
            for (i = 0u; i < old_count; ++i) {
                if ((mask & (1u << i)) == 0u) continue;
                while (j < new_count && new_chars[j] != old_chars[i]) ++j;
                if (j == new_count) break;
                ++j;
                ++matched;
            }
            if (i == old_count && matched > best) best = matched;
        }
        CHECK(unchanged == best);
        CHECK(added == new_count - best && deleted == old_count - best);
        xrtFree(stats);
        xrtFree(output);
    }
    return 0;
}

int main(void)
{
    char line[2049];
    char many[4102];
    char* held;
    char* output;
    char* large_input;
    int i;

    CHECK(same(xdiff_lines("a\nb\nc", "a\nB\nc"),
        " a\n-b\n+B\n c\n"));
    CHECK(same(xdiff_stats("a\nb\nc", "a\nB\nc"), "1,1,2"));
    CHECK(same(xdiff_lines("", ""), ""));
    CHECK(same(xdiff_stats("", ""), "0,0,0"));
    CHECK(xdiff_lines(NULL, "") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xdiff_lines("", NULL) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xdiff_stats(NULL, "") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xdiff_stats("", NULL) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(same(xdiff_lines("a\r\nb\r\n", "a\nb"), " a\n b\n"));
    CHECK(same(xdiff_lines("你好\n旧", "你好\n新"),
        " 你好\n-旧\n+新\n"));

    /* 文本接口不能将非法 UTF-8 字节原样交回 XLang string。 */
    CHECK(xdiff_lines("\xc3\x28", "ok") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xdiff_stats("ok", "\xe4\xb8") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(same(xdiff_lines("ok", "ok"), " ok\n"));
    CHECK(xrtGetError() == NULL);

    memset(line, 'q', sizeof(line) - 1u);
    line[sizeof(line) - 1u] = '\0';
    output = (char*)xdiff_lines("", line);
    CHECK(output != NULL && strlen(output) == strlen(line) + 2u);
    CHECK(output[0] == '+' && output[strlen(output) - 1u] == '\n');
    xrtFree(output);

    /* 多次结果并存，旧结果不能被下一次调用覆盖。 */
    held = (char*)xdiff_lines("old", "new");
    CHECK(held != NULL);
    for (i = 0; i < 128; ++i) {
        output = (char*)xdiff_lines("x", "y");
        CHECK(output != NULL && strcmp(held, "-old\n+new\n") == 0);
        xrtFree(output);
    }
    xrtFree(held);

    /* 单次输出超过旧实现 16 KiB 上限，仍须完整返回。 */
    large_input = (char*)xrtMalloc(20001u);
    CHECK(large_input != NULL);
    memset(large_input, 'z', 20000u);
    large_input[20000] = '\0';
    output = (char*)xdiff_lines("", large_input);
    CHECK(output != NULL && strlen(output) == 20002u && output[20001] == '\n');
    xrtFree(output);
    xrtFree(large_input);

    for (i = 0; i < 2048; ++i) {
        many[2 * i] = 'a';
        many[2 * i + 1] = '\n';
    }
    many[4096] = '\0';
    CHECK(same(xdiff_stats(many, ""), "0,2048,0"));
    /* 两侧都到行数上限时，完整 LCS 表仍须给出正确结果。 */
    CHECK(same(xdiff_stats(many, many), "0,0,2048"));
    output = (char*)xdiff_lines(many, many);
    CHECK(output != NULL && strlen(output) == 2048u * 3u &&
        memcmp(output, " a\n", 3u) == 0 &&
        memcmp(output + 2047u * 3u, " a\n", 3u) == 0);
    xrtFree(output);
    for (i = 2048; i < 2050; ++i) {
        many[2 * i] = 'a';
        many[2 * i + 1] = '\n';
    }
    many[4100] = '\0';
    CHECK(xdiff_lines(many, "") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(same(xdiff_stats("a", "a"), "0,0,1"));
    CHECK(xrtGetError() == NULL);
    CHECK(xdiff_stats(many, "") == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(same(xdiff_lines("a", "b"), "-a\n+b\n"));
    CHECK(xrtGetError() == NULL);
    CHECK(test_allocation_failure() == 0);
    CHECK(test_lcs_properties() == 0);
    CHECK(test_owned_result_budget() == 0);
    puts("diff C boundaries: PASS");
    return 0;
}
