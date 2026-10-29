#include "xregex.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    xregex* words = xrtRegexCompile(xrtStrView("(?<word>[a-z]+)-([0-9]+)"));
    xregexmatcher* matcher = xrtRegexMatcherCreate(words);
    xregexresult result;
    xregexcapture word, number;
    xregex* digits;
    str replaced;
    xregex* comma;
    xregexsplitter* splitter;
    xregexsplitconfig config;
    xregexsplitpart part;
    const xstrview patterns[] = {{"error", 5}, {"warn(ing)?", 9}, {"[0-9]+", 6}};
    xregexset* set;
    xregexsetmatcher* categories;
    size_t i;

    /* C 端 matcher 借用输入；字符串常量在本例中始终有效。 */
    assert(words && matcher);
    result = xrtRegexMatcherFind(matcher, xrtStrView("ab-1 cd-22"), 0);
    while (result == XREGEX_MATCH) {
        assert(xrtRegexMatcherCapture(matcher, 1, &word));
        assert(xrtRegexMatcherCapture(matcher, 2, &number));
        printf("word=%.*s, id=%.*s\n", (int)word.Text.Size, word.Text.Data,
            (int)number.Text.Size, number.Text.Data);
        result = xrtRegexMatcherNext(matcher);
    }
    assert(result == XREGEX_NONE);
    xrtRegexMatcherFree(matcher);
    xrtRegexRelease(words);

    digits = xrtRegexCompile(xrtStrView("[0-9]+"));
    assert(digits);
    replaced = xrtRegexReplace(digits, xrtStrView("a1 b22"), xrtStrView("<$0>"));
    assert(replaced);
    puts(replaced);
    xrtFree(replaced);
    xrtRegexRelease(digits);

    comma = xrtRegexCompile(xrtStrView(","));
    xrtRegexSplitConfigInit(&config);
    splitter = xrtRegexSplitterCreate(comma, xrtStrView("left,right"), &config);
    assert(comma && splitter);
    while ((result = xrtRegexSplitterNext(splitter, &part)) == XREGEX_MATCH)
        printf("field=%.*s\n", (int)part.Text.Size, part.Text.Data);
    assert(result == XREGEX_NONE);
    xrtRegexSplitterFree(splitter);
    xrtRegexRelease(comma);

    /* 集合一次执行全部模式，返回按模式序号排列的命中结果。 */
    set = xrtRegexSetCompile(patterns, sizeof(patterns) / sizeof(patterns[0]));
    categories = xrtRegexSetMatcherCreate(set);
    assert(set && categories);
    assert(xrtRegexSetMatcherMatch(categories, xrtStrView("warning 42"), 0) == XREGEX_MATCH);
    for (i = 0; i < xrtRegexSetMatcherCount(categories); ++i)
        printf("pattern=%u\n", (unsigned)xrtRegexSetMatcherIndex(categories, i));
    xrtRegexSetMatcherFree(categories);
    xrtRegexSetRelease(set);
    return 0;
}
