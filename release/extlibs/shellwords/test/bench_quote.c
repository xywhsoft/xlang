/* 性能单独运行；同一语料/编译器/运行库比较旧、新原生实现，不计作功能门禁。 */
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static bool sample(const char* name, const char* input, const xvalue* words)
{
    uint64 calls = 0, checksum = 0;
    clock_t start = clock(), end;
    if (start == (clock_t)-1) return false;
    do {
        for (unsigned i = 0; i != 64; ++i) {
            str result = words != NULL ? xrtShellwordsJoin(words) : xrtShellwordsQuote(input);
            if (result == NULL) return false;
            checksum += strlen(result) + (unsigned char)result[0];
            xrtFree(result);
            ++calls;
        }
        end = clock();
        if (end == (clock_t)-1) return false;
    } while ((double)(end - start) / CLOCKS_PER_SEC < 0.125);
    printf("%s\t%.12f\t%llu\t%llu\n", name,
        (double)(end - start) / CLOCKS_PER_SEC / (double)calls,
        (unsigned long long)calls, (unsigned long long)(checksum / calls));
    return true;
}

int main(void)
{
    char input[8193];
    static const char* names[] = {"safe", "spaces", "sparse-quotes", "dense-quotes", "bytes"};
    for (unsigned mode = 0; mode != 5; ++mode) {
        for (unsigned i = 0; i != 8192; ++i)
            input[i] = mode == 0 ? 'a' : (mode == 1 ? ' ' :
                (mode == 2 ? (i % 97 == 0 ? '\'' : 'a') :
                (mode == 3 ? '\'' : (char)(i % 255 + 1))));
        input[8192] = 0;
        if (!sample(names[mode], input, NULL)) return 1;
    }
    xvalue* words = xrtValueArray();
    if (words == NULL) return 1;
    memset(input, ' ', 128);
    for (unsigned i = 0; i != 128; ++i) {
        xvalue* value = xrtValueString((xstrview){input, 128});
        if (value == NULL || !xrtValueArrayAppendTake(words, &value)) {
            xrtValueRelease(value);
            xrtValueRelease(words);
            return 1;
        }
    }
    bool ok = sample("join-128", NULL, words);
    xrtValueRelease(words);
    return ok ? 0 : 1;
}
