/* 构造哈希位向量，直接覆盖随机样本几乎不可能碰到的最大秩和饱和分支。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static uint64 given_hash;
static size_t expected_length = 3;
static unsigned bad_inputs, checks;
static uint64 vectors;

static uint64 fixture_hash(const void *data, size_t size) {
    if (size != expected_length || (size != 0 && memcmp(data, "a\0b", size) != 0)) ++bad_inputs;
    return given_hash;
}

/* 替换仅存在于本测试翻译单元，正式库仍调用原 XRT 哈希。 */
#define xrtHash64 fixture_hash
#include "../hyperloglog_native.c"
#undef xrtHash64

#define CHECK(test) do { ++checks; if (!(test)) { \
    fprintf(stderr, "HLL vector line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

static int feed(xhll_state *state, unsigned bucket, unsigned rank) {
    unsigned bits = 64 - state->precision;
    given_hash = ((uint64)bucket << bits) | (rank > bits ? 0 : UINT64_C(1) << (bits - rank));
    ++vectors;
    return xhll_add(state, "a\0b", 3);
}

/* 均匀秩时以整数有理数精确舍入，不复用被测的浮点谐波求和。 */
static int64_t uniform_count(unsigned m, unsigned rank) {
    uint64 numerator, denominator;
    if (m <= 64) {
        unsigned coefficient = m == 16 ? 673 : (m == 32 ? 697 : 709);
        numerator = (uint64)coefficient * m * (1u << rank);
        denominator = 1000;
    } else {
        numerator = UINT64_C(7213) * m * m * (1u << rank);
        denominator = UINT64_C(10) * (1000u * m + 1079u);
    }
    return (int64_t)((numerator + denominator / 2) / denominator);
}

static int register_edges(unsigned precision) {
    xhll_state *state = (xhll_state *)xhll_create((int)precision);
    unsigned rank, i, positions[4];
    CHECK(state != NULL && xhll_precision(state) == (int)precision);
    positions[0] = 0; positions[1] = 1;
    positions[2] = state->nregisters / 2; positions[3] = state->nregisters - 1;
    CHECK(xhll_count(state) == 0 && xhll_added(state) == 0);
    CHECK(xrtMemDebugFailAfter(0));
    for (rank = 1; rank <= 65 - precision; ++rank) {
        for (i = 0; i < 4; ++i) {
            CHECK(feed(state, positions[i], rank));
            CHECK(state->registers[positions[i]] == rank);
            CHECK(feed(state, positions[i], 1));
            CHECK(state->registers[positions[i]] == rank);
        }
        CHECK(xhll_added(state) == (int64_t)(rank * 8));
        CHECK(xhll_count(state) == (precision == 4 ? 5 : 4));
    }
    CHECK(xhll_clear(state) && xhll_added(state) == 0);
    for (rank = 1; rank <= 2; ++rank) {
        for (i = 0; i < state->nregisters; ++i) {
            CHECK(feed(state, i, rank));
            CHECK(state->registers[i] == rank);
        }
        CHECK(xhll_count(state) == uniform_count(state->nregisters, rank));
    }
    /* 有限大估计仍须返回有限 int64，不能把 32 位边界当成饱和线。 */
    for (i = 0; i < state->nregisters; ++i) {
        CHECK(feed(state, i, 63 - precision));
        CHECK(state->registers[i] == 63 - precision);
    }
    CHECK(xhll_count(state) > INT64_C(6000000000000000000));
    CHECK(xhll_count(state) < INT64_C(7000000000000000000));
    for (i = 0; i < state->nregisters; ++i) {
        CHECK(feed(state, i, 65 - precision));
        CHECK(state->registers[i] == 65 - precision);
    }
    CHECK(xhll_count(state) == INT64_MAX);
    CHECK(xhll_added(state) == (int64_t)state->nregisters * 4);
    CHECK(xhll_clear(state) && xhll_count(state) == 0);
    /* 同一缓冲的显式长度包含 NUL，-1 只读取第一个 NUL 前的字节。 */
    expected_length = 1;
    CHECK(xhll_add(state, "a\0b", -1));
    expected_length = 0;
    CHECK(xhll_add(state, "", 0));
    expected_length = 3;
    CHECK(xhll_added(state) == 2 && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xhll_destroy(state);
    return 0;
}

static int merge_edges(unsigned precision) {
    xhll_state *left = (xhll_state *)xhll_create((int)precision);
    xhll_state *right = (xhll_state *)xhll_create((int)precision);
    void *wrong = xhll_create(precision == 18 ? 17 : (int)precision + 1);
    unsigned i;
    int64_t estimate;
    CHECK(left != NULL && right != NULL && wrong != NULL);
    CHECK(xrtMemDebugFailAfter(0));
    for (i = 0; i < left->nregisters; ++i) {
        CHECK(feed(left, i, 1 + i % 3));
        CHECK(feed(right, i, 1 + (i + 1) % 5));
    }
    CHECK(xhll_merge(left, right));
    for (i = 0; i < left->nregisters; ++i) {
        unsigned a = 1 + i % 3, b = 1 + (i + 1) % 5;
        CHECK(left->registers[i] == (a > b ? a : b));
        CHECK(right->registers[i] == b);
    }
    estimate = xhll_count(left);
    CHECK(xhll_added(left) == (int64_t)left->nregisters * 2);
    CHECK(xhll_merge(left, right) && xhll_merge(left, left));
    CHECK(xhll_count(left) == estimate && xhll_added(left) == (int64_t)left->nregisters * 3);
    CHECK(xhll_added(right) == right->nregisters && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(!xhll_merge(left, wrong) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    for (i = 0; i < left->nregisters; ++i) {
        unsigned a = 1 + i % 3, b = 1 + (i + 1) % 5;
        CHECK(left->registers[i] == (a > b ? a : b) && right->registers[i] == b);
    }
    CHECK(xhll_count(left) == estimate && xhll_added(left) == (int64_t)left->nregisters * 3);
    CHECK(xhll_count(wrong) == 0 && xhll_added(wrong) == 0);
    xhll_destroy(wrong);
    xhll_destroy(right);
    xhll_destroy(left);
    return 0;
}

int main(void) {
    unsigned precision;
    xmemdebugsnapshot before, after;
    void *warm = xhll_create(4);
    CHECK(warm != NULL && xhll_count(warm) == 0);
    xhll_destroy(warm);
    xrtMemDebugSnapshot(&before);
    CHECK(before.Enabled);
    for (precision = 4; precision <= 18; ++precision) {
        CHECK(register_edges(precision) == 0);
        CHECK(merge_edges(precision) == 0);
    }
    CHECK(bad_inputs == 0);
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    CHECK(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    CHECK(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    printf("HLL controlled hashes: %llu operations, %u checks, 15 precisions PASS\n",
        (unsigned long long)vectors, checks);
    return 0;
}
