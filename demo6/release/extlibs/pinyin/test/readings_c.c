#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/pinyin_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../pinyin_native.c"
#endif

/* 预期来自独立的 Unihan 数据；不从产品字表读取预期读音。 */
static const struct { unsigned code; const char *reading; } expected[] = {
#include "readings_expected.inc"
};
_Static_assert(sizeof(expected) / sizeof(expected[0]) == 395, "fixed vocabulary");
static unsigned checks, failures;

static void compare(const char *input, size_t size, const char *want, int initials) {
    char *actual = initials ? xpinyin_initials_n(input, size) : xpinyin_convert_n(input, size);
    ++checks;
    if (!actual || strcmp(actual, want) != 0 || xrtGetError() != NULL) {
        if (failures < 8) fprintf(stderr, "reading %u mode=%d: expected %s, actual %s\n",
            checks, initials, want, actual ? actual : "NULL");
        ++failures;
    }
    xrtFree(actual);
}

/* 独立编码测试输入，不借用产品的解码/查表实现。 */
static size_t encode(unsigned scalar, char text[5]) {
    size_t size;
    if (scalar < 0x80) { text[0] = (char)scalar; size = 1; }
    else if (scalar < 0x800) {
        text[0] = (char)(0xc0 | (scalar >> 6));
        text[1] = (char)(0x80 | (scalar & 63)); size = 2;
    } else if (scalar < 0x10000) {
        text[0] = (char)(0xe0 | (scalar >> 12));
        text[1] = (char)(0x80 | ((scalar >> 6) & 63));
        text[2] = (char)(0x80 | (scalar & 63)); size = 3;
    } else {
        text[0] = (char)(0xf0 | (scalar >> 18));
        text[1] = (char)(0x80 | ((scalar >> 12) & 63));
        text[2] = (char)(0x80 | ((scalar >> 6) & 63));
        text[3] = (char)(0x80 | (scalar & 63)); size = 4;
    }
    text[size] = 0;
    return size;
}

int main(void) {
    xmemdebugsnapshot before, after;
    size_t position = 0;
    unsigned scalar;
    static const unsigned edges[] = {1, 0x7f, 0x80, 0x7ff, 0x800, 0x33ff,
        0xa000, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x1f600, 0x20000, 0x10ffff};
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 1; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        if (expected[i-1].code >= expected[i].code) return 2;
    }

    /* 整段汉字区同时核对395条映射和所有未收录码点，覆盖二分边界。 */
    for (scalar = 0x3400; scalar <= 0x9fff; ++scalar) {
        char input[5], initial[2];
        size_t size = encode(scalar, input);
        const char *want = input, *short_want = input;
        if (position < sizeof(expected) / sizeof(expected[0]) && expected[position].code == scalar) {
            want = expected[position++].reading;
            initial[0] = want[0]; initial[1] = 0; short_want = initial;
        }
        compare(input, size, want, 0);
        compare(input, size, short_want, 1);
    }
    if (position != sizeof(expected) / sizeof(expected[0])) return 2;
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        char input[5];
        size_t size = encode(edges[i], input);
        compare(input, size, input, 0); compare(input, size, input, 1);
    }
    compare("伉佛女", sizeof("伉佛女")-1, "kang fo nv", 0);
    compare("伉佛女", sizeof("伉佛女")-1, "kfn", 1);
    compare("A伉龘B", sizeof("A伉龘B")-1, "A kang 龘 B", 0);
    compare("A伉龘B", sizeof("A伉龘B")-1, "Ak龘B", 1);
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) return 3;
    printf("pinyin readings: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
