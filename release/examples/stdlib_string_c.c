#define XRT_MODULE_NUMBER_FORMAT
#define XRT_MODULE_STRING_GLOB
#define XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_UNICODE_DISTANCE
#define XRT_MODULE_UNICODE_TEXT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 比较两个允许为空的零结尾字符串。 */
static int str_eq(const char* left, const char* right)
{
    if ( left == NULL ) {
        left = "";
    }
    if ( right == NULL ) {
        right = "";
    }
    return strcmp(left, right) == 0;
}



/* 比较字符串视图与零结尾字符串。 */
static int view_eq(xstrview text, const char* expected)
{
    xstrview right = xrtStrView(expected);

    return xrtStrEqual(text, right);
}



/* 输出一个与 xlang 示例相同的验证结果。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 验证并释放 XRT 返回的所有权字符串。 */
static int check_owned_text(char* text, const char* expected)
{
    int ok = str_eq(text, expected);

    xrtFree(text);
    return ok;
}



/* 按 Unicode 标量集合裁剪字符串并返回独立副本。 */
static char* trim_text(const char* text, const char* chars, int side)
{
    xstrview textView = xrtStrView(text);
    xstrview charsView = xrtStrView(chars);
    xstrview result;
    bool ok = true;

    if ( charsView.Size == 0 ) {
        result = side < 0 ? xrtStrTrimLeft(textView) :
            (side > 0 ? xrtStrTrimRight(textView) : xrtStrTrim(textView));
    } else if ( side < 0 ) {
        ok = xrtUtf8TrimLeftSet(textView, charsView, &result);
    } else if ( side > 0 ) {
        ok = xrtUtf8TrimRightSet(textView, charsView, &result);
    } else {
        ok = xrtUtf8TrimSet(textView, charsView, &result);
    }
    return ok ? xrtStrDupView(result) : NULL;
}



/* 验证严格 UTF-8 解码产生预期标量序列。 */
static int check_chars_array(void)
{
    xstrview text = XRT_STR_LITERAL("A你");
    const uint32 expected[] = { 'A', 0x4F60u };
    size_t position = 0;
    size_t count = 0;

    while ( position < text.Size ) {
        uint32 scalar;
        size_t read;

        if ( (count >= (sizeof(expected) / sizeof(expected[0]))) ||
             (xrtUtf8Decode(xrtStrViewN(
                text.Data + position, text.Size - position),
                &scalar, &read) != XUTF_OK) ||
             (scalar != expected[count]) ) {
            return 0;
        }
        position += read;
        count++;
    }
    return count == (sizeof(expected) / sizeof(expected[0]));
}



/* 验证 UTF-8 的原始字节视图。 */
static int check_bytes_array(void)
{
    const unsigned char* text = (const unsigned char*)"A你";

    return strlen((const char*)text) == 4 &&
           text[0] == 65 &&
           text[1] == 228 &&
           text[2] == 189 &&
           text[3] == 160;
}



/* 验证逐标量迭代时的索引行为。 */
static int check_iteration(void)
{
    xstrview text = XRT_STR_LITERAL("A你B");
    size_t position = 0;
    long long index = 0;
    long long indexSum = 0;
    size_t charCount = 0;

    while ( position < text.Size ) {
        uint32 scalar;
        size_t read;

        if ( xrtUtf8Decode(xrtStrViewN(
                text.Data + position, text.Size - position),
                &scalar, &read) != XUTF_OK ) {
            return 0;
        }
        (void)scalar;
        position += read;
        indexSum += index;
        index++;
        charCount++;
    }
    return charCount == 3 && indexSum == 3;
}



/* 使用当前 XRT API 完成与 xlang 标准库示例相同的操作。 */
int main(void)
{
    const char* title = "XLang DEMO6";
    const char* utf = "1234你好";
    xstrview joinItems[] = {
        XRT_STR_LITERAL("red"),
        XRT_STR_LITERAL("green"),
        XRT_STR_LITERAL("blue")
    };
    xstrlist* parts;
    xstrlist* lines;
    char* joined;

    require_line("length",
        xrtUtf8Count(xrtStrView(utf)) == 6 &&
        xrtStrView(utf).Size == 10);

    require_line("search",
        xrtStrCaseCompare(xrtStrView(title),
            XRT_STR_LITERAL("xlang demo6")) == 0 &&
        xrtStrCaseContains(xrtStrView(title), XRT_STR_LITERAL("lang")) &&
        xrtUtf8Find(xrtStrView(title), XRT_STR_LITERAL("DEMO"), 0) == 6 &&
        xrtUtf8RFind(XRT_STR_LITERAL("ab你好cd你好"),
            XRT_STR_LITERAL("你好")) == 6 &&
        xrtStrCount(XRT_STR_LITERAL("ab你好cd你好"),
            XRT_STR_LITERAL("你好")) == 2 &&
        xrtStrCaseStarts(xrtStrView(title), XRT_STR_LITERAL("xlang")) &&
        xrtStrEnds(xrtStrView(title), XRT_STR_LITERAL("6")) &&
        xrtStrContainsAny(xrtStrView(title), XRT_STR_LITERAL(" -_")) &&
        xrtStrGlob(XRT_STR_LITERAL("readme.md"),
            XRT_STR_LITERAL("*.md"), 0) &&
        xrtUtf8Similarity(XRT_STR_LITERAL("xlang"),
            XRT_STR_LITERAL("xlang")) >= 0.95 &&
        xrtUtf8Similarity(xrtStrView(title),
            XRT_STR_LITERAL("XLang DEMO6")) == 1.0 &&
        xrtUtf8Valid(XRT_STR_LITERAL("中文 UTF-8"), NULL));

    require_line("format",
        check_owned_text(xrtIntFormat(1234567,
            XRT_STR_LITERAL(",")), "1,234,567") &&
        check_owned_text(xrtIntFormat(42,
            XRT_STR_LITERAL("04")), "0042") &&
        check_owned_text(xrtNumFormat(12.345,
            XRT_STR_LITERAL(".2")), "12.35") &&
        check_owned_text(xrtNumFormat(0.125,
            XRT_STR_LITERAL(".1%")), "12.5%"));

    require_line("case_trim",
        check_owned_text(xrtStrLower(xrtStrView(title)), "xlang demo6") &&
        check_owned_text(xrtStrUpper(xrtStrView(title)), "XLANG DEMO6") &&
        check_owned_text(trim_text("  xlang  ", "", 0), "xlang") &&
        check_owned_text(trim_text("  xlang  ", "", -1), "xlang  ") &&
        check_owned_text(trim_text("  xlang  ", "", 1), "  xlang") &&
        check_owned_text(trim_text("--xlang--", "-", 0), "xlang") &&
        check_owned_text(xrtUtf8Filter(XRT_STR_LITERAL("x-lang-demo"),
            XRT_STR_LITERAL("-")), "xlangdemo") &&
        check_owned_text(xrtStrReplace(XRT_STR_LITERAL("a-b-c"),
            XRT_STR_LITERAL("-"), XRT_STR_LITERAL("/")), "a/b/c"));

    require_line("slice",
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("ab你好cd"), 2, 2), "你好") &&
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("ab你好cd"), -2, -1), "cd") &&
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("ab你好"), 0, 3), "ab你") &&
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("ab你好"), -2, -1), "你好") &&
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("ab你好cd"), 2, 2), "你好") &&
        check_owned_text(xrtUtf8Substr(
            XRT_STR_LITERAL("A你B"), 1, 1), "你") &&
        (unsigned char)xrtStrView("A你").Data[1] == 228);

    joined = xrtStrJoin(XRT_STR_LITERAL("|"), joinItems,
        sizeof(joinItems) / sizeof(joinItems[0]));
    require_line("edit",
        check_owned_text(joined, "red|green|blue") &&
        check_owned_text(xrtStrRepeat(
            XRT_STR_LITERAL("ha"), 3), "hahaha") &&
        xrtStrEmpty(XRT_STR_LITERAL("")) &&
        !xrtStrEmpty(XRT_STR_LITERAL(" ")) &&
        xrtStrBlank(XRT_STR_LITERAL(" \t")) &&
        !xrtStrBlank(XRT_STR_LITERAL(" x ")) &&
        check_owned_text(xrtStrReplace(XRT_STR_LITERAL("a-b-c"),
            XRT_STR_LITERAL("-"), XRT_STR_LITERAL("")), "abc") &&
        check_owned_text(xrtUtf8Remove(
            XRT_STR_LITERAL("ab你好cd"), 2, 2), "abcd") &&
        check_owned_text(xrtUtf8Insert(XRT_STR_LITERAL("abcd"),
            2, XRT_STR_LITERAL("你好")), "ab你好cd") &&
        check_owned_text(xrtUtf8PadLeft(XRT_STR_LITERAL("x"),
            3, XRT_STR_LITERAL(".")), "..x") &&
        check_owned_text(xrtUtf8PadRight(XRT_STR_LITERAL("x"),
            3, XRT_STR_LITERAL(".")), "x..") &&
        check_owned_text(xrtUtf8PadCenter(XRT_STR_LITERAL("x"),
            5, XRT_STR_LITERAL(".")), "..x..") &&
        check_owned_text(xrtUtf8Reverse(
            XRT_STR_LITERAL("ab你好")), "好你ba"));

    parts = xrtStrSplit(
        XRT_STR_LITERAL("x-lang-demo"), XRT_STR_LITERAL("-"));
    require_line("split",
        parts != NULL &&
        parts->Count == 3 &&
        view_eq(parts->Items[0], "x") &&
        view_eq(parts->Items[1], "lang") &&
        view_eq(parts->Items[2], "demo"));
    xrtStrListFree(parts);

    lines = xrtStrSplitLines(XRT_STR_LITERAL("a\nb\nc"));
    if ( !(lines != NULL &&
           lines->Count == 3 &&
           view_eq(lines->Items[0], "a") &&
           view_eq(lines->Items[1], "b") &&
           view_eq(lines->Items[2], "c")) ) {
        fprintf(stderr, "split=fail\n");
        exit(1);
    }
    xrtStrListFree(lines);

    require_line("arrays", check_chars_array() && check_bytes_array());
    require_line("iteration", check_iteration());
    return 0;
}
