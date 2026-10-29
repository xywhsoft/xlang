#include "../include/langdetect_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void)
{
    static const struct { const char* name; const char* pattern; size_t target; unsigned iterations; } cases[] = {
        {"short", "The garden is nice and the sky is blue.", 64, 30000},
        {"long-token", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 65536, 32},
        {"word-rich", "the and is of to in that with this for ", 65536, 32},
        {"separators", "a0123456789_-.,; !? 0123456789___ ", 65536, 32},
        {"utf8-edges", "theé éthe the× ×the the÷ the😀 unknown ", 65536, 32},
        {"han", "你好世界，这是中文测试。", 65536, 32},
        {"limit", "the garden is nice and the sky is blue. ", 1048576, 3}
    };
    char* input = malloc(1048577);
    if (!input) return 2;
    for (size_t i = 0; i < sizeof(cases)/sizeof(*cases); ++i) {
        size_t unit = strlen(cases[i].pattern), size = 0;
        uint64_t checksum = 0;
        while (size+unit <= cases[i].target) { memcpy(input+size, cases[i].pattern, unit); size += unit; }
        if (!size) { free(input); return 2; }
        // 只比较同一C入口、输入、编译器和优化选项；计时不作为功能门禁。
        (void)xlangdetect_detect(input, size);
        clock_t start = clock();
        for (unsigned n = 0; n < cases[i].iterations; ++n) {
            const char* result = xlangdetect_detect(input, size);
            checksum += (unsigned char)result[n%strlen(result)];
        }
        clock_t end = clock();
        if (start == (clock_t)-1 || end == (clock_t)-1 || end < start) { free(input); return 2; }
        printf("%s,%zu,%u,%.6f,%" PRIu64 "\n", cases[i].name, size, cases[i].iterations,
            (double)(end-start)/CLOCKS_PER_SEC, checksum);
    }
    free(input);
    return 0;
}

#ifdef __TINYC__
#include "../langdetect_native.c"
#endif
