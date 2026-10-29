#define XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_NUMBER_FORMAT
#include "xrt.h"
#include <assert.h>
#include <stdio.h>

/* C 直接使用 XRT，对照语言层的字符、字节与拥有权语义。 */
int main(void)
{
    xstrview text = xrtStrView("A你好B");
    char* slice = xrtUtf8Substr(text, 1, 2);
    char* replaced = xrtStrReplace(text, xrtStrView("你好"), xrtStrView("hello"));
    xstrlist* parts = xrtStrSplit(xrtStrView("red||你好"), xrtStrView("|"));
    xstrlist* lines = xrtStrSplitLines(xrtStrView("first\r\nsecond\nthird"));
    size_t i;
    assert(slice != NULL && replaced != NULL && parts != NULL && lines != NULL);
    printf("%zu\n%zu\n%d\n%s\n%s\n", xrtUtf8Count(text), text.Size,
        (int)(unsigned char)text.Data[1], slice, replaced);
    printf("%d\n%d\n", (int)xrtStrCaseStarts(xrtStrView("Content-Type"), xrtStrView("content")),
        (int)xrtStrCaseEnds(xrtStrView("report.TXT"), xrtStrView(".txt")));
    for ( i = 0; i < parts->Count; ++i )
        printf("[%.*s]\n", (int)parts->Items[i].Size, parts->Items[i].Data);
    for ( i = 0; i < lines->Count; ++i )
        printf("%.*s\n", (int)lines->Items[i].Size, lines->Items[i].Data);
    {
        // C 明确提供视图，拼接算法与 xlang 共用 XRT。
        int64 answer = 42;
        bool success = true;
        char* number = xrtIntFormat(answer, xrtStrView(""));
        xstrview values[] = {xrtStrView("answer"), xrtStrView(number),
            xrtStrView(success ? "true" : "false"), xrtStrView("null")};
        char* joined = xrtStrJoin(xrtStrView(" / "), parts->Items, parts->Count);
        char* mixed = xrtStrJoin(xrtStrView(":"), values, 4);
        assert(number != NULL && joined != NULL && mixed != NULL);
        printf("%s\n%s\n", joined, mixed);
        xrtFree(mixed);
        xrtFree(joined);
        xrtFree(number);
    }
    xrtStrListFree(lines);
    xrtStrListFree(parts);
    xrtFree(replaced);
    xrtFree(slice);
    {
        // C 用相同的 XRT 解码器遍历字符，字节遍历无需解码或分配。
        xstrview rest = xrtStrView("A你");
        xstrview raw = rest;
        while (rest.Size != 0) {
            uint32 scalar;
            size_t size;
            assert(xrtUtf8Decode(rest, &scalar, &size) == XUTF_OK);
            printf("%.*s\n", (int)size, rest.Data);
            rest.Data += size;
            rest.Size -= size;
        }
        for (i = 0; i < raw.Size; ++i) printf("%u\n", (unsigned char)raw.Data[i]);
    }
    return 0;
}
