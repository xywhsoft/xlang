#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int check_owned_text(char* text, const char* expected)
{
    int ok = str_eq(text, expected);
    xrtFree(text);
    return ok;
}

static char* join3(const char* a, const char* b, const char* c, const char* sep)
{
    size_t sepLen = strlen(sep);
    size_t aLen = strlen(a);
    size_t bLen = strlen(b);
    size_t cLen = strlen(c);
    size_t len = aLen + bLen + cLen + sepLen * 2;
    char* result = (char*)xrtMalloc(len + 1);
    size_t pos = 0;

    if ( result == NULL ) {
        return NULL;
    }
    memcpy(result + pos, a, aLen);
    pos += aLen;
    memcpy(result + pos, sep, sepLen);
    pos += sepLen;
    memcpy(result + pos, b, bLen);
    pos += bLen;
    memcpy(result + pos, sep, sepLen);
    pos += sepLen;
    memcpy(result + pos, c, cLen);
    pos += cLen;
    result[pos] = 0;
    return result;
}

static int check_chars_array(void)
{
    const char* text = "A你";
    size_t len = strlen(text);
    size_t pos = 0;
    size_t count = 0;

    while ( pos < len ) {
        size_t charLen = xrtStrUtf8CharSize((char*)text, len, pos);
        if ( charLen == 0 ) {
            return 0;
        }
        if ( count == 0 && !(charLen == 1 && text[pos] == 'A') ) {
            return 0;
        }
        if ( count == 1 && !(charLen == 3 && memcmp(text + pos, "你", 3) == 0) ) {
            return 0;
        }
        pos += charLen;
        count++;
    }
    return count == 2;
}

static int check_bytes_array(void)
{
    const unsigned char* text = (const unsigned char*)"A你";
    return strlen((const char*)text) == 4 &&
           text[0] == 65 &&
           text[1] == 228 &&
           text[2] == 189 &&
           text[3] == 160;
}

static int check_iteration(void)
{
    const char* text = "A你B";
    size_t len = strlen(text);
    size_t pos = 0;
    long long index = 0;
    long long indexSum = 0;
    size_t charCount = 0;

    while ( pos < len ) {
        size_t charLen = xrtStrUtf8CharSize((char*)text, len, pos);
        if ( charLen == 0 ) {
            return 0;
        }
        pos += charLen;
        indexSum += index;
        index++;
        charCount++;
    }
    return charCount == 3 && indexSum == 3;
}

int main(void)
{
    const char* title = "XLang DEMO6";
    const char* utf = "1234你好";
    char** parts;
    char** lines;
    size_t count = 0;
    char* joined;

    xrtInit();

    require_line("length",
        xrtStrCharLen((char*)utf, 0) == 6 &&
        xrtStrByteLen((char*)utf, 0) == 10);

    require_line("search",
        xrtStrComp((char*)title, "xlang demo6", 0, TRUE) == 0 &&
        xrtStrComp((char*)title, "xlang demo6", 0, TRUE) == 0 &&
        xrtInStr((char*)title, 0, "lang", 0, TRUE) != 0 &&
        xrtStrIndexOf((char*)title, 0, "DEMO", 0, FALSE) == 6 &&
        xrtStrLastIndexOf("ab你好cd你好", 0, "你好", 0, FALSE) == 6 &&
        xrtStrCount("ab你好cd你好", 0, "你好", 0, FALSE) == 2 &&
        xrtStrIndexOf((char*)title, 0, "xlang", 0, TRUE) == 0 &&
        xrtStrComp((char*)title + strlen(title) - 1, "6", 0, FALSE) == 0 &&
        xrtCheckStr((char*)title, 0, " -_", 0) != NULL &&
        xrtStrLike("readme.md", 0, "*.md", 0, FALSE) &&
        xrtStrApprox("xlang", 0, "xlang", 0) &&
        xrtStrSim((char*)title, 0, "XLang DEMO6", 0) == 1.0 &&
        xrtIsUTF8("中文 UTF-8", 0));

    require_line("format",
        check_owned_text(xrtIntFormat(1234567, ","), "1,234,567") &&
        check_owned_text(xrtIntFormat(42, "04"), "0042") &&
        check_owned_text(xrtNumFormat(12.345, ".2"), "12.35") &&
        check_owned_text(xrtNumFormat(0.125, ".1%"), "12.5%"));

    require_line("case_trim",
        check_owned_text(xrtLCase((char*)title, 0, FALSE), "xlang demo6") &&
        check_owned_text(xrtUCase((char*)title, 0, FALSE), "XLANG DEMO6") &&
        check_owned_text(xrtTrim("  xlang  ", 0, "", 0, FALSE, NULL), "xlang") &&
        check_owned_text(xrtLTrim("  xlang  ", 0, "", 0, FALSE, NULL), "xlang  ") &&
        check_owned_text(xrtRTrim("  xlang  ", 0, "", 0, FALSE, NULL), "  xlang") &&
        check_owned_text(xrtTrim("--xlang--", 0, "-", 0, FALSE, NULL), "xlang") &&
        check_owned_text(xrtFilterStr("x-lang-demo", 0, "-", 0, FALSE, NULL), "xlangdemo") &&
        check_owned_text(xrtReplace("a-b-c", 0, "-", 0, "/", 0, NULL), "a/b/c"));

    require_line("slice",
        check_owned_text(xrtStrSlice("ab你好cd", 0, 2, 2, NULL), "你好") &&
        check_owned_text(xrtStrSlice("ab你好cd", 0, -2, -1, NULL), "cd") &&
        check_owned_text(xrtStrLeft("ab你好", 0, 3, NULL), "ab你") &&
        check_owned_text(xrtStrRight("ab你好", 0, 2, NULL), "你好") &&
        check_owned_text(xrtStrMid("ab你好cd", 0, 2, 2, NULL), "你好") &&
        check_owned_text(xrtStrCharAt("A你B", 0, 1, NULL), "你") &&
        xrtStrByteAt("A你", 0, 1) == 228);

    joined = join3("red", "green", "blue", "|");
    require_line("edit",
        check_owned_text(joined, "red|green|blue") &&
        check_owned_text(xrtStrRepeat("ha", 0, 3, NULL), "hahaha") &&
        xrtStrIsEmpty("", 0) &&
        !xrtStrIsEmpty(" ", 0) &&
        xrtStrIsBlank(" \t", 0) &&
        !xrtStrIsBlank(" x ", 0) &&
        check_owned_text(xrtStrRemove("a-b-c", 0, "-", 0, FALSE, NULL), "abc") &&
        check_owned_text(xrtStrRemoveAt("ab你好cd", 0, 2, 2, NULL), "abcd") &&
        check_owned_text(xrtStrInsert("abcd", 0, 2, "你好", 0, NULL), "ab你好cd") &&
        check_owned_text(xrtStrPadLeft("x", 0, 3, ".", 0, NULL), "..x") &&
        check_owned_text(xrtStrPadRight("x", 0, 3, ".", 0, NULL), "x..") &&
        check_owned_text(xrtStrPadCenter("x", 0, 5, ".", 0, NULL), "..x..") &&
        check_owned_text(xrtStrReverse("ab你好", 0, NULL), "好你ba"));

    parts = xrtSplit("x-lang-demo", 0, "-", 0, FALSE, &count);
    require_line("split",
        parts != NULL &&
        count == 3 &&
        str_eq(parts[0], "x") &&
        str_eq(parts[1], "lang") &&
        str_eq(parts[2], "demo"));
    xrtFree(parts);

    lines = xrtStrSplitLines("a\nb\nc", 0, &count);
    if ( !(lines != NULL && count == 3 && str_eq(lines[0], "a") && str_eq(lines[1], "b") && str_eq(lines[2], "c")) ) {
        fprintf(stderr, "split=fail\n");
        exit(1);
    }
    xrtFree(lines);

    require_line("arrays", check_chars_array() && check_bytes_array());
    require_line("iteration", check_iteration());

    xrtUnit();
    return 0;
}
