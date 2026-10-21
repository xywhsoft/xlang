#define XRT_MODULE_MEMORY_DEBUG
#include "../include/pinyin_native.h"
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "pinyin C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int check_text(const char *input, const char *expected, int initials) {
    char *result = (char *)(initials ? xpinyin_initials(input) : xpinyin_convert(input));
    int equal = result != NULL && strcmp(result, expected) == 0;
    xrtFree(result);
    return equal;
}

static int allocation_errors(void) {
    char text[601];
    unsigned failure, rejected;
    int mode, i;
    for (i = 0; i < 200; ++i) memcpy(text + 3 * i, "中", 3);
    text[600] = 0;
    for (mode = 0; mode < 2; ++mode) {
        rejected = 0u;
        for (failure = 0u; failure < 24u; ++failure) {
            char *result;
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            result = (char *)(mode ? xpinyin_initials(text) : xpinyin_convert(text));
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(result == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                xrtClearError();
                CHECK(check_text("中国", mode ? "zg" : "zhong guo", mode));
                ++rejected;
            } else {
                CHECK(result != NULL && strlen(result) == (mode ? 200u : 1199u));
                xrtFree(result);
                break;
            }
        }
        CHECK(rejected >= 1u && failure < 24u);
    }
    return 0;
}

int main(void) {
    char *long_input = (char *)malloc(3 * 5000 + 1);
    char *long_result;
    char *first;
    char *second;
    char *recovered;
    int index;
    CHECK(long_input != NULL);

    CHECK(check_text("中国", "zhong guo", 0));
    CHECK(check_text("中国", "zg", 1));
    CHECK(check_text("hello中国123", "hello zhong guo 123", 0));
    CHECK(check_text("hello中国123", "hellozg123", 1));
    CHECK(check_text("龘中", "龘 zhong", 0));
    CHECK(check_text("龘中", "龘z", 1));
    CHECK(check_text("😀", "😀", 0));
    CHECK(check_text("音乐", "yin le", 0));
    CHECK(check_text("", "", 0));
    CHECK(check_text("", "", 1));

    /* 返回值必须各自持有内存，后续调用不得覆写前一次结果。 */
    first = (char *)xpinyin_convert("中国");
    second = (char *)xpinyin_convert("你好");
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(strcmp(first, "zhong guo") == 0 && strcmp(second, "ni hao") == 0);
    xrtFree(first);
    xrtFree(second);

    for (index = 0; index < 5000; ++index) memcpy(long_input + index * 3, "中", 3);
    long_input[3 * 5000] = 0;
    long_result = (char *)xpinyin_initials(long_input);
    CHECK(long_result != NULL && strlen(long_result) == 5000);
    CHECK(long_result[0] == 'z' && long_result[4999] == 'z');
    xrtFree(long_result);
    free(long_input);

    CHECK(xpinyin_convert_n("a\0b", 3) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    recovered = (char *)xpinyin_initials("中国");
    CHECK(recovered != NULL && strcmp(recovered, "zg") == 0);
    CHECK(xrtGetError() == NULL);
    xrtFree(recovered);
    xrtClearError();
    CHECK(xpinyin_convert_n("\xc0\xaf", 2) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    CHECK(xpinyin_initials_n("\xed\xa0\x80", 3) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    {
        const char badTail[] = "中国\xc0\xaf";
        const char hiddenNul[] = "中国\0tail";
        CHECK(xpinyin_convert_n(badTail, sizeof(badTail) - 1u) == NULL &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        CHECK(xpinyin_initials_n(hiddenNul, sizeof(hiddenNul) - 1u) == NULL &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        CHECK(check_text("中国", "zhong guo", 0) && xrtGetError() == NULL);
    }
    xrtClearError();
    CHECK(xpinyin_convert(NULL) == NULL);
    xrtClearError();
    CHECK(allocation_errors() == 0);
    puts("pinyin C boundaries: PASS");
    return 0;
}
