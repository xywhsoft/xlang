/* 与 lexical_text.xl 同输入、同断言、同黄金输出；不执行任何命令。 */
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <string.h>

static bool equals(const xvalue* values, size_t index, cstr expected)
{
    xstrview text;
    return xrtValueGetString(xrtValueArrayGet(values, index), &text) &&
        text.Size == strlen(expected) && memcmp(text.Data, expected, text.Size) == 0;
}

int main(void)
{
    xvalue* words = xrtShellwordsSplit("tool pre' two'\" parts\" '' \"a\\qb\" a\\\nb 'x\\\ny'");
    bool ok = xrtGetError() == NULL && xrtValueCount(words) == 6 &&
        equals(words, 1, "pre two parts") && equals(words, 2, "") && equals(words, 3, "a\\qb") &&
        equals(words, 4, "ab") && equals(words, 5, "x\\\ny");
    xrtValueRelease(words);
    if (!ok) return 1;
    const char* invalid[] = {"'unfinished", "tail\\"};
    for (size_t i = 0; i < 2; ++i) {
        words = xrtShellwordsSplit(invalid[i]);
        ok = xrtValueType(words) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
        xrtValueRelease(words);
        if (!ok) return 1;
    }
    /* 成功入口自行清除上次错误；C 调用者也能复用同一套恢复语义。 */
    words = xrtShellwordsSplit("recovered");
    ok = xrtGetError() == NULL && equals(words, 0, "recovered");
    xrtValueRelease(words);
    if (!ok) return 1;
    puts("argc: 6\nmerged: pre two parts\nempty: true\nliteral slash: true\ncontinuation: ab\nsingle quoted newline: true\nrecovery: true");
    return 0;
}

#include "../shellwords_native.c"
