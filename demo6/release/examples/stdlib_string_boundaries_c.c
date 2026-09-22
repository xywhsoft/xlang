#define XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_UNICODE_TEXT
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
    xrtStrListFree(lines);
    xrtStrListFree(parts);
    xrtFree(replaced);
    xrtFree(slice);
    return 0;
}
