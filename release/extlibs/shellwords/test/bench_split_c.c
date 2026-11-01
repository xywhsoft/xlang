/* 手工微基准，不进入功能门禁；旧/新版本链接相同的非调试 XRT。 */
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    static const char* names[] = {"plain", "single", "double", "escapes", "many", "short"};
    char input[16388];
    for (unsigned mode = 0; mode < 6; ++mode) {
        size_t n = 0;
        if (mode == 1) input[n++] = '\'';
        if (mode == 2) input[n++] = '"';
        if (mode == 5) { memcpy(input, "tool 'two words' tail", 21); n = 21; }
        else for (size_t i = 0; i < (mode == 4 ? 512u : 16384u); ++i)
            input[n++] = mode == 3 ? "a\\ b"[i % 4] : mode == 4 ? "abc "[i % 4] : mode == 0 ? 'a' : "a b"[i % 3];
        if (mode == 1) input[n++] = '\'';
        if (mode == 2) input[n++] = '"';
        input[n] = 0;
        unsigned count = mode == 5 ? 20000 : 1000;
        size_t checksum = 0;
        clock_t start = clock();
        for (unsigned i = 0; i < count; ++i) {
            xvalue* value = xrtShellwordsSplit(input);
            if (xrtValueType(value) != XVALUE_ARRAY || xrtGetError() != NULL) return 1;
            checksum += xrtValueCount(value);
            xrtValueRelease(value);
        }
        printf("%s\t%zu\t%u\t%.9f\t%zu\n", names[mode], n, count,
            (double)(clock() - start) / CLOCKS_PER_SEC, checksum);
    }
    return 0;
}
