#define XRT_MODULE_MEMORY_DEBUG
#include "../include/color_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* expected[] = {
    "#010203", "#01020304", "#AABBCC", "#FF0000",
    "#00FF00", "#808080", "#FAFAD2", "lightgoldenrodyellow"
};

/* 只在测试中归一调用签名，公开 C 接口仍直接写调用者缓冲。 */
static bool write_text(int operation, bool invalid, char* output, size_t capacity)
{
    switch (operation) {
    case 0: return xcolor_hex(invalid ? INT64_MAX : 1, 2, 3, output, capacity);
    case 1: return xcolor_hex_a(1, 2, 3, invalid ? INT64_MIN : 4, output, capacity);
    case 2: return xcolor_normalize(invalid ? "#bad!" : "#abc", output, capacity);
    case 3: return xcolor_from_hsv(invalid ? NAN : 0, 1, 1, output, capacity);
    case 4: return xcolor_from_hsl(120, invalid ? INFINITY : 1, 0.5, output, capacity);
    case 5: return xcolor_mix("#000", invalid ? "#bad!" : "#fff", 0.5, output, capacity);
    case 6: return xcolor_from_name(invalid ? "missing" : "LightGoldenRodYellow", output, capacity);
    default: return xcolor_to_name(invalid ? "#010203" : "#FAFAD2", output, capacity);
    }
}

static unsigned buffer_contracts(void)
{
    unsigned checks = 0;
    for (int operation = 0; operation < 8; ++operation) {
        for (int invalid = 0; invalid < 2; ++invalid) {
            for (size_t capacity = 0; capacity <= XCOLOR_NAME_SIZE; ++capacity) {
                char storage[32], wanted[32];
                memset(storage, 0x5a, sizeof(storage));
                memcpy(wanted, storage, sizeof(storage));
                size_t size = strlen(expected[operation]) + 1u;
                bool fits = !invalid && capacity >= size;
                if (fits) memcpy(wanted + 1, expected[operation], size);
                else if (capacity) wanted[1] = '\0';
                xrtSetErrorInfo(XERR_STATE, "color.test", 9, "stale marker");
                assert(write_text(operation, invalid != 0, storage + 1, capacity) == fits);
                assert(memcmp(storage, wanted, sizeof(storage)) == 0);
                if (!capacity) assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
                else if (!invalid && !fits) assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
                else assert(xrtGetError() == NULL);
                ++checks;
            }
            /* 无论输入是否合法，缺少输出缓冲都应报告参数错误。 */
            for (size_t capacity = 0; capacity <= XCOLOR_NAME_SIZE; capacity += XCOLOR_NAME_SIZE) {
                assert(!write_text(operation, invalid != 0, NULL, capacity));
                assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
                ++checks;
            }
        }
    }
    return checks;
}

static unsigned overlap_contracts(void)
{
    const int operations[] = {2, 5, 6, 7};
    const char* inputs[] = {"#abc", "#000", "LightGoldenRodYellow", "#FAFAD2"};
    unsigned checks = 0;
    for (size_t i = 0; i < 4; ++i) {
        for (int offset = -8; offset <= 8; ++offset) {
            char storage[80], wanted[80];
            memset(storage, 0x5a, sizeof(storage));
            strcpy(storage + 24, inputs[i]);
            memcpy(wanted, storage, sizeof(storage));
            size_t at = (size_t)(24 + offset);
            memcpy(wanted + at, expected[operations[i]], strlen(expected[operations[i]]) + 1u);
            bool ok;
            switch (operations[i]) {
            case 2: ok = xcolor_normalize(storage + 24, storage + at, sizeof(storage) - at); break;
            case 5: ok = xcolor_mix(storage + 24, "#fff", 0.5, storage + at, sizeof(storage) - at); break;
            case 6: ok = xcolor_from_name(storage + 24, storage + at, sizeof(storage) - at); break;
            default: ok = xcolor_to_name(storage + 24, storage + at, sizeof(storage) - at); break;
            }
            assert(ok && xrtGetError() == NULL);
            assert(memcmp(storage, wanted, sizeof(storage)) == 0);
            ++checks;
        }
    }
    /* 输出还可能同时覆盖两个输入，必须先读完二者再写。 */
    const size_t offsets[] = {8, 12, 13, 16, 18, 20, 24};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        char storage[48], wanted[48];
        memset(storage, 0x5a, sizeof(storage));
        strcpy(storage + 12, "#000");
        strcpy(storage + 18, "#fff");
        memcpy(wanted, storage, sizeof(storage));
        memcpy(wanted + offsets[i], "#808080", 8);
        assert(xcolor_mix(storage + 12, storage + 18, 0.5,
            storage + offsets[i], sizeof(storage) - offsets[i]));
        assert(memcmp(storage, wanted, sizeof(storage)) == 0);
        ++checks;
    }
    return checks;
}

static char* empty_result(int operation)
{
    switch (operation) {
    case 0: return xcolor_hex_text(INT64_MAX, 2, 3);
    case 1: return xcolor_hex_a_text(1, 2, 3, INT64_MIN);
    case 2: return xcolor_normalize_text("#bad!");
    case 3: return xcolor_from_hsv_text(NAN, 1, 1);
    case 4: return xcolor_from_hsl_text(120, INFINITY, 0.5);
    case 5: return xcolor_mix_text("#000", "#bad!", 0.5);
    case 6: return xcolor_from_name_text("missing");
    default: return xcolor_to_name_text("#010203");
    }
}

static void owned_empty_contracts(void)
{
    for (int operation = 0; operation < 8; ++operation) {
        assert(xrtMemDebugFailAfter(0));
        assert(empty_result(operation) == NULL && xrtMemDebugFailTriggered());
        assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        char* first = empty_result(operation);
        char* second = empty_result(operation);
        assert(first && second && first != second);
        assert(first[0] == '\0' && second[0] == '\0' && xrtGetError() == NULL);
        xrtFree(first);
        assert(second[0] == '\0');
        xrtFree(second);
    }
}

int main(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    unsigned buffers = buffer_contracts();
    unsigned overlaps = overlap_contracts();
    owned_empty_contracts();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    printf("color contracts: %u buffers, %u overlaps, 8 owned-empty OOM/recovery PASS\n", buffers, overlaps);
    return 0;
}
