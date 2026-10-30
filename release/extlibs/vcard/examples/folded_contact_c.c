#define XRT_MODULE_UNICODE
#define XRT_MODULE_FILE_WHOLE
#include "../include/vcard_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../vcard_native.c"
#include "../vcard_parse.c"
#include "../vcard_build.c"
#endif

static xvalue* field(xvalue* card, const char* key)
{
    return xrtValueObjectGet(card, (xstrview){key, strlen(key)});
}

/* 与 XLang 同一输入、映射与输出；C 显式释放两个拥有型结果。 */
int main(int argc, char** argv)
{
    if (argc > 2) return 2;
    const char source[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:张\r\n\t三\r\n"
        "item1.EMAIL:home@\r\n example.com\r\n"
        "NOTE:第一行\\n第二\r\n 行\r\nEND:VCARD\r\n";
    xvalue* card = xv_parse(source, sizeof(source) - 1);
    char* canonical = card ? xv_build(card) : NULL;
    xstrview name = {0}, email = {0}, note = {0};
    bool ok = canonical && xrtUtf8Valid((xstrview){canonical, strlen(canonical)}, NULL) &&
        xrtValueGetString(field(card, "fn"), &name) &&
        xrtValueGetString(field(card, "email"), &email) &&
        xrtValueGetString(field(card, "note"), &note);
    if (ok) ok = name.Size == strlen("张三") && !memcmp(name.Data, "张三", name.Size) &&
        email.Size == 16 && !memcmp(email.Data, "home@example.com", 16) &&
        note.Size == strlen("第一行\n第二行") && !memcmp(note.Data, "第一行\n第二行", note.Size);
    if (ok && argc == 2) ok = xrtFileWriteAll(argv[1], (xbytesview){(cbytes)canonical, strlen(canonical)});
    if (ok) printf("name: %.*s\nemail: %.*s\nnote restored: true\n",
        (int)name.Size, name.Data, (int)email.Size, email.Data);
    xrtFree(canonical);
    xrtValueRelease(card);
    return ok ? 0 : 1;
}
