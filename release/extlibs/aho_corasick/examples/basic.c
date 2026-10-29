#include "../include/xrt_aho_corasick.h"
#include <stdio.h>

/* C 与 xlang 范例使用相同输入、匹配顺序和输出文本。 */
static bool print_match(const xrtahomatch* match, void* data)
{
    bool* pause = data;
    printf("%.*s:%llu:%llu\n", (int)match->Pattern.Size, match->Pattern.Data,
        (unsigned long long)match->iStart, (unsigned long long)match->iEnd);
    if (pause && *pause) { *pause = false; return false; }
    return true;
}

static bool print_owned(const char* label, str text)
{
    if (!text) { return false; }
    printf("%s%s\n", label, text);
    xrtFree(text);
    return true;
}

int main(void)
{
    const xstrview patterns[] = { XRT_STR_INIT("he"), XRT_STR_INIT("she"),
        XRT_STR_INIT("his"), XRT_STR_INIT("hers") };
    xrtaho* ac = xrtAhoCreate(NULL);
    xrtaho* unicode = NULL;
    xrtaho* folded = NULL;
    xrtahoresult* result = NULL;
    xrtahostate* state = NULL;
    xrtahomatch match;
    xrtahooptions options;
    uint64 count;
    bool found, pause = true;
    int exitCode = 1;
    if (!ac || !xrtAhoAddMany(ac, patterns, 4) || !xrtAhoBuild(ac)) { goto cleanup; }

    puts("sorted:");
    result = xrtAhoSearch(ac, XRT_STR_LITERAL("ushers"));
    if (!result) { goto cleanup; }
    for (size_t i = 0; i < xrtAhoResultCount(result); ++i) {
        if (!xrtAhoResultAt(result, i, &match)) { goto cleanup; }
        print_match(&match, NULL);
    }
    if (!xrtAhoFind(ac, XRT_STR_LITERAL("ushers"), &match, &found) || !found) { goto cleanup; }
    printf("first=%.*s\n", (int)match.Pattern.Size, match.Pattern.Data);
    if (!xrtAhoCount(ac, XRT_STR_LITERAL("ushers"), &count)) { goto cleanup; }
    printf("count=%llu\n", (unsigned long long)count);
    if (!print_owned("replace=", xrtAhoReplace(ac, XRT_STR_LITERAL("ushers"), XRT_STR_LITERAL("*"), NULL)) ||
        !print_owned("highlight=", xrtAhoHighlight(ac, XRT_STR_LITERAL("ushers"), XRT_STR_LITERAL("<"), XRT_STR_LITERAL(">"), NULL)) ||
        !print_owned("mask=", xrtAhoMask(ac, XRT_STR_LITERAL("ushers"), XRT_STR_LITERAL("#"), NULL))) { goto cleanup; }

    /* 分块不改变全局字节偏移；暂停后继续提交剩余切片。 */
    puts("stream:");
    state = xrtAhoStateCreate(ac);
    if (!state || xrtAhoStateFeed(state, XRT_STR_LITERAL("us"), print_match, &pause, NULL) != XRT_AHO_SCAN_COMPLETE) { goto cleanup; }
    xstrview chunk = XRT_STR_INIT("hers");
    xrtahoscanstatus status;
    do {
        size_t consumed;
        status = xrtAhoStateFeed(state, chunk, print_match, &pause, &consumed);
        if (status == XRT_AHO_SCAN_ERROR) { goto cleanup; }
        chunk.Data += consumed;
        chunk.Size -= consumed;
    } while (status == XRT_AHO_SCAN_STOPPED);

    /* Unicode 遮盖按字符数，匹配 start/end 仍按 UTF-8 字节偏移。 */
    unicode = xrtAhoCreate(NULL);
    if (!unicode || !xrtAhoAdd(unicode, XRT_STR_LITERAL("你好")) ||
        !xrtAhoAdd(unicode, XRT_STR_LITERAL("好世")) || !xrtAhoBuild(unicode) ||
        !print_owned("unicode=", xrtAhoMask(unicode, XRT_STR_LITERAL("甲你好世界"), XRT_STR_LITERAL("*"), NULL))) { goto cleanup; }
    xrtAhoOptionsInit(&options);
    options.bIgnoreAsciiCase = true;
    folded = xrtAhoCreate(&options);
    if (!folded || !xrtAhoAdd(folded, XRT_STR_LITERAL("HTTP")) || !xrtAhoBuild(folded) ||
        !xrtAhoCount(folded, XRT_STR_LITERAL("http"), &count)) { goto cleanup; }
    printf("ascii=%llu\n", (unsigned long long)count);
    exitCode = 0;
cleanup:
    if (exitCode) { fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError())); }
    xrtAhoDestroy(folded);
    xrtAhoDestroy(unicode);
    xrtAhoStateDestroy(state);
    xrtAhoResultDestroy(result);
    xrtAhoDestroy(ac);
    return exitCode;
}
