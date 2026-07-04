#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static size_t text_byte_len(const char* text)
{
    return xrtStrByteLen((str)text, 0);
}

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

int main(void)
{
    const char* text = "item-2026-ready";
    xregex* compiled;
    xregex* regex;
    xregexmatch* match;
    xregexmatch* shifted;
    xregexmatch* missing;
    const char* patterns[] = {"[0-9]+", "ready"};
    xregexset* set;
    uint32 indexes[4];
    uint32 count = 0;
    xregexspan span;
    str matchText;
    str shiftedText;
    int basicOk = 1;

    xrtInit();

    regex = xrtRegexCreateEx("^item-[0-9]+-[a-z]+$", 0, 0);
    basicOk = basicOk &&
        regex != NULL &&
        xrtRegexIsMatch(regex, text, text_byte_len(text)) > 0 &&
        xrtRegexFullMatch(regex, text, text_byte_len(text)) > 0;
    xrtRegexDestroy(regex);

    regex = xrtRegexCreateEx("^item-[0-9]+-[a-z]+$", 0, XRT_REGEX_FLAG_INSENSITIVE);
    basicOk = basicOk &&
        regex != NULL &&
        xrtRegexIsMatch(regex, "ITEM-2026-READY", text_byte_len("ITEM-2026-READY")) > 0;
    xrtRegexDestroy(regex);

    regex = xrtRegexCreateEx("[0-9]+", 0, 0);
    span.iBegin = 0;
    span.iEnd = 0;
    basicOk = basicOk &&
        regex != NULL &&
        xrtRegexFind(regex, text, text_byte_len(text), &span) > 0 &&
        span.iBegin == 5 &&
        span.iEnd == 9;
    xrtRegexDestroy(regex);

    regex = xrtRegexCreateEx("a+", 0, XRT_REGEX_FLAG_UNGREEDY);
    basicOk = basicOk &&
        regex != NULL &&
        xrtRegexIsMatch(regex, "aa", 2) > 0;
    xrtRegexDestroy(regex);

    regex = xrtRegexCreateEx("a.b", 0, XRT_REGEX_FLAG_DOTNEWLINE);
    basicOk = basicOk &&
        regex != NULL &&
        xrtRegexFullMatch(regex, "a\nb", 3) > 0 &&
        (XRT_REGEX_FLAG_INSENSITIVE + XRT_REGEX_FLAG_MULTILINE + XRT_REGEX_FLAG_DOTNEWLINE + XRT_REGEX_FLAG_UNGREEDY) >= 15;
    xrtRegexDestroy(regex);
    require_line("basic", basicOk);

    compiled = xrtRegexCreateEx("[0-9]+", 0, 0);
    match = xrtRegexFindMatchAt(compiled, text, text_byte_len(text), 0);
    matchText = xrtRegexMatchTextCopy(match);
    require_line("match",
        xrtRegexMatchOk(match) &&
        xrtRegexMatchByteStart(match) == 5 &&
        xrtRegexMatchByteEnd(match) == 9 &&
        xrtRegexMatchStart(match) == 5 &&
        xrtRegexMatchEnd(match) == 9 &&
        xrtStrComp(matchText, "2026", 0, FALSE) == 0);
    xrtFree(matchText);

    shifted = xrtRegexFindMatchAt(compiled, "a1 b2", 5, 2);
    missing = xrtRegexFindMatchAt(compiled, "missing", 7, 0);
    shiftedText = xrtRegexMatchTextCopy(shifted);
    require_line("lazy",
        xrtRegexMatchOk(shifted) &&
        xrtRegexMatchStart(shifted) == 4 &&
        xrtStrComp(shiftedText, "2", 0, FALSE) == 0 &&
        !xrtRegexMatchOk(missing) &&
        xrtRegexMatchStart(missing) == -1 &&
        xrtRegexMatchEnd(missing) == -1 &&
        xrtRegexMatchText(missing) == NULL);
    xrtFree(shiftedText);
    xrtRegexMatchDestroy(match);
    xrtRegexMatchDestroy(shifted);
    xrtRegexMatchDestroy(missing);
    xrtRegexDestroy(compiled);

    set = xrtRegexSetCreateEx(patterns, 2, XRT_REGEX_FLAG_INSENSITIVE);
    require_line("set",
        xrtRegexSetIsMatchAt(set, "READY", 5, 0) > 0 &&
        xrtRegexSetFirstAt(set, "READY", 5, 0) == 1 &&
        xrtRegexSetFirstAt(set, "code 2026", 9, 0) == 0 &&
        xrtRegexSetMatchesAt(set, "READY 2026", 10, 0, indexes, 4, &count) > 0 &&
        count == 2 &&
        (indexes[0] == 0 || indexes[1] == 0) &&
        (indexes[0] == 1 || indexes[1] == 1));
    xrtRegexSetDestroy(set);

    xrtUnit();
    return 0;
}
