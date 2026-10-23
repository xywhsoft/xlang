#define XRT_MODULE_UNICODE_DISTANCE
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/fuzzy_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "fuzzy C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int reference_distance(const unsigned char* left, int left_count,
    const unsigned char* right, int right_count)
{
    int previous[13], current[13];
    int i, j;
    for (j = 0; j <= right_count; ++j) previous[j] = j;
    for (i = 1; i <= left_count; ++i) {
        current[0] = i;
        for (j = 1; j <= right_count; ++j) {
            int deletion = previous[j] + 1;
            int insertion = current[j - 1] + 1;
            int substitution = previous[j - 1] + (left[i - 1] != right[j - 1]);
            int smallest = deletion < insertion ? deletion : insertion;
            current[j] = smallest < substitution ? smallest : substitution;
        }
        for (j = 0; j <= right_count; ++j) previous[j] = current[j];
    }
    return previous[right_count];
}

static int test_distance_reference(void)
{
    static const char* const symbols[] = {
        "a", "b", "\xe4\xbd\xa0", "\xf0\x9f\x99\x82"
    };
    uint64_t seed = UINT64_C(0x243f6a8885a308d3);
    int sample;

    /* 字符索引作为独立模型，避免把 UTF-8 字节长度误当编辑距离。 */
    for (sample = 0; sample < 2048; ++sample) {
        unsigned char left_ids[12], right_ids[12];
        char left[49], right[49];
        size_t left_bytes = 0, right_bytes = 0;
        int left_count, right_count, distance, maximum, index;

        seed = seed * UINT64_C(6364136223846793005) + 1;
        left_count = (int)((seed >> 32) % 13);
        seed = seed * UINT64_C(6364136223846793005) + 1;
        right_count = (int)((seed >> 32) % 13);
        for (index = 0; index < left_count; ++index) {
            const char* symbol;
            size_t length;
            seed = seed * UINT64_C(6364136223846793005) + 1;
            left_ids[index] = (unsigned char)((seed >> 32) % 4);
            symbol = symbols[left_ids[index]];
            length = strlen(symbol);
            memcpy(left + left_bytes, symbol, length);
            left_bytes += length;
        }
        left[left_bytes] = '\0';
        for (index = 0; index < right_count; ++index) {
            const char* symbol;
            size_t length;
            seed = seed * UINT64_C(6364136223846793005) + 1;
            right_ids[index] = (unsigned char)((seed >> 32) % 4);
            symbol = symbols[right_ids[index]];
            length = strlen(symbol);
            memcpy(right + right_bytes, symbol, length);
            right_bytes += length;
        }
        right[right_bytes] = '\0';
        distance = reference_distance(left_ids, left_count, right_ids, right_count);
        maximum = left_count > right_count ? left_count : right_count;
        CHECK(xfuzzy_levenshtein(left, right) == distance);
        CHECK(xfuzzy_ratio(left, right) ==
            (maximum == 0 ? 100 : (maximum - distance) * 100 / maximum));
    }
    return 0;
}

static int test_best_reference(void)
{
    static const char* const patterns[] = {
        "abc", "pc", "你", "FOO", "xyz", ""
    };
    static const char* const candidates[] = {
        "abc", "a_b_c", "abcx", "lib/parser.c", "src/main.c",
        "你好", "你们", "foobar", "foo-bar", "", "zzz"
    };
    uint32_t seed = UINT32_C(0x9e3779b9);
    unsigned trial;
    /* 独立遍历候选，核对 best 的最高分和并列首项规则。 */
    for (trial = 0; trial < 512u; ++trial) {
        const char* pattern = patterns[trial % (sizeof(patterns) / sizeof(patterns[0]))];
        const char* expected = "";
        int64_t best_score = -1;
        char list[256];
        size_t used = 0u;
        unsigned count = 1u + trial % 6u, i;
        char* actual;
        for (i = 0; i < count; ++i) {
            const char* candidate;
            size_t length;
            int64_t score;
            seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
            candidate = candidates[(seed >> 16) %
                (sizeof(candidates) / sizeof(candidates[0]))];
            length = strlen(candidate);
            CHECK(used + length + 3u < sizeof(list));
            memcpy(list + used, candidate, length);
            used += length;
            if (i + 1u < count || (trial & 1u) != 0u) {
                if ((trial & 2u) != 0u) list[used++] = '\r';
                list[used++] = '\n';
            }
            score = xfuzzy_match(pattern, candidate);
            CHECK(score >= -1);
            if (score > best_score) {
                best_score = score;
                expected = candidate;
            }
        }
        list[used] = '\0';
        actual = (char*)xfuzzy_best(pattern, list);
        CHECK(actual != NULL && strcmp(actual, expected) == 0);
        CHECK(xrtGetError() == NULL);
        xrtFree(actual);
    }
    return 0;
}

int main(void)
{
    char long_text[5001];
    char late_candidate[9008];
    char too_long[4098];
    char* oversized;
    char* held;
    char* result;
    int i;

    CHECK(xfuzzy_levenshtein("kitten", "sitting") == 3);
    CHECK(xfuzzy_levenshtein("你好", "你们") == 1);
    CHECK(xfuzzy_ratio("你好", "你们") == 50);
    CHECK(xfuzzy_ratio("", "") == 100);
    if (test_distance_reference()) return 1;
    if (test_best_reference()) return 1;
    CHECK(xfuzzy_match("fb", "foobar") >= 0);
    CHECK(xfuzzy_match("FOO", "foobar") >= 0);
    CHECK(xfuzzy_match("你", "你好") >= 0);
    CHECK(xfuzzy_match("汉", "你好") == -1);
    CHECK(xfuzzy_match("xyz", "foobar") == -1);
    CHECK(xfuzzy_match("ab", "zzzzab") == 8);
    CHECK(xfuzzy_match("\xe4\xbd\xa0" "a", "zzz\xe4\xbd\xa0" "a") == 8);
    CHECK(xfuzzy_match("ab", "zzzz") == -1);
    CHECK(xfuzzy_match("abc", "abc") > xfuzzy_match("abc", "a_b_c"));
    result = (char*)xfuzzy_best("abc", "a_b_c\nabc");
    CHECK(result != NULL && strcmp(result, "abc") == 0);
    xrtFree(result);
    result = (char*)xfuzzy_best("abc", "abcx\nabc");
    CHECK(result != NULL && strcmp(result, "abc") == 0);
    xrtFree(result);

    result = (char*)xfuzzy_best("pc", "src/main.c\nlib/parser.c\r\nlib/lexer.c");
    CHECK(result != NULL && strcmp(result, "lib/parser.c") == 0);
    xrtFree(result);
    result = (char*)xfuzzy_best("missing", "abc\ndef");
    CHECK(result != NULL && strcmp(result, "") == 0);
    xrtFree(result);
    result = (char*)xfuzzy_best("x", "x\r");
    CHECK(result != NULL && strcmp(result, "x\r") == 0);
    xrtFree(result);
    result = (char*)xfuzzy_best("x", "x\r\n");
    CHECK(result != NULL && strcmp(result, "x") == 0);
    xrtFree(result);

    /* 超过旧实现的 4096 字节结果缓冲，结果仍保持完整。 */
    memset(long_text, 'x', 5000u);
    long_text[5000] = '\0';
    result = (char*)xfuzzy_best("x", long_text);
    CHECK(result != NULL && strlen(result) == 5000u);
    xrtFree(result);

    /* 列表越过旧实现的 8192 字节后，后面的候选仍可参与选择。 */
    memset(late_candidate, 'x', 9000u);
    late_candidate[9000] = '\n';
    memcpy(late_candidate + 9001, "target", 7u);
    result = (char*)xfuzzy_best("target", late_candidate);
    CHECK(result != NULL && strcmp(result, "target") == 0);
    xrtFree(result);

    held = (char*)xfuzzy_best("pc", "lib/parser.c\nsrc/main.c");
    CHECK(held != NULL);
    for (i = 0; i < 100; ++i) {
        result = (char*)xfuzzy_best("a", "a\nb");
        CHECK(result != NULL && strcmp(held, "lib/parser.c") == 0);
        xrtFree(result);
    }
    xrtFree(held);

    memset(too_long, 'q', 4097u);
    too_long[4097] = '\0';
    CHECK(xfuzzy_levenshtein(too_long, "") == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xfuzzy_match("\xc3\x28", "abc") == -2);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xfuzzy_best("a", "\xc3\x28") == NULL);

    oversized = (char*)xrtMalloc(1024u * 1024u + 2u);
    CHECK(oversized != NULL);
    memset(oversized, 'x', 1024u * 1024u);
    oversized[1024u * 1024u] = '\0';
    result = (char*)xfuzzy_best("x", oversized);
    CHECK(result != NULL && strlen(result) == 1024u * 1024u);
    xrtFree(result);
    oversized[1024u * 1024u] = 'x';
    oversized[1024u * 1024u + 1u] = '\0';
    CHECK(xfuzzy_match("x", oversized) == -2);
    CHECK(xfuzzy_best("x", oversized) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(oversized);

    /* 失败后成功或正常不匹配均不能保留旧线程错误。 */
    CHECK(xfuzzy_ratio("abc", "abc") == 100 && xrtGetError() == NULL);
    CHECK(xfuzzy_levenshtein(too_long, "") == -1 && xrtGetError() != NULL);
    CHECK(xfuzzy_match("q", "abc") == -1 && xrtGetError() == NULL);
    CHECK(xfuzzy_match("\xc3\x28", "abc") == -2 && xrtGetError() != NULL);
    result = (char*)xfuzzy_best("pc", "lib/parser.c\nsrc/main.c");
    CHECK(result != NULL && strcmp(result, "lib/parser.c") == 0);
    xrtFree(result);
    CHECK(xrtGetError() == NULL);

    xrtClearError();
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xfuzzy_best("pc", "lib/parser.c\nsrc/main.c") == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtClearError();
    result = (char*)xfuzzy_best("pc", "lib/parser.c\nsrc/main.c");
    CHECK(result != NULL && strcmp(result, "lib/parser.c") == 0);
    xrtFree(result);

    puts("fuzzy C boundaries: PASS");
    return 0;
}
