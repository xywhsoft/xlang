#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 易变的是函数指针本身，避免测试被编译器折叠为常量比较。 */
static size_t (*volatile crt_length)(const char*) = strlen;

static int verify(const char* text, size_t expected)
{
    size_t actual = crt_length(text);
    if (actual == expected) return 0;
    fprintf(stderr, "CRT strlen: expected=%zu actual=%zu\n", expected, actual);
    return 1;
}

int main(void)
{
    static const char embedded[] = {'a', '\0', 'b', '\0'};
    static const size_t sizes[] = {0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 4095, 4096};
    char storage[4160], before[4160];
    unsigned checks = 0;
    for (size_t offset = 0; offset < 32; ++offset) {
        for (size_t n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
            memset(storage, 0x5a, sizeof(storage));
            for (size_t i = 0; i < sizes[n]; ++i) storage[offset + i] = (char)(1 + i % 255);
            storage[offset + sizes[n]] = '\0';
            memcpy(before, storage, sizeof(storage));
            if (verify(storage + offset, sizes[n]) || memcmp(storage, before, sizeof(storage))) return 1;
            ++checks;
        }
    }
    /* 刚好分配到终止符；Linux ASan 可检查终止位置和边界读取。 */
    for (size_t n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
        char* text = malloc(sizes[n] + 1);
        if (!text) return 1;
        memset(text, 'x', sizes[n]);
        text[sizes[n]] = '\0';
        int failed = verify(text, sizes[n]);
        free(text);
        if (failed) return 1;
        ++checks;
    }
    if (verify("", 0) || verify("hello", 5) || verify("你好", 6) ||
        sizeof(embedded) - 1 != 3 || verify(embedded, 1)) return 1;
    printf("cString CRT: %u runtime buffers, empty/UTF-8/NUL PASS\n", checks);
    return 0;
}
