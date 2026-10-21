#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "../include/vcard_native.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char expected[] = "BEGIN:VCARD\r\nVERSION:3.0\r\n"
        "FN:王小明\r\nN:王;小明;;;\r\n"
        "EMAIL:home@example.com\r\nEMAIL:work@example.com\r\n"
        "END:VCARD\r\n";
    xvalue *card = xrtValueObject();
    xvalue *emails = xrtValueArray();
    xvalue *back = NULL;
    xvalue *name;
    xstrview text;
    char *built = NULL;
    int status = 1;
    if (argc > 2 || card == NULL || emails == NULL) goto done;
    if (!xrtValueObjectSetNew(card, (xstrview){"fn", 2},
            xrtValueString((xstrview){"王小明", strlen("王小明")})) ||
        !xrtValueObjectSetNew(card, (xstrview){"n", 1},
            xrtValueString((xstrview){"王;小明;;;", strlen("王;小明;;;")})) ||
        !xrtValueArrayAppendNew(emails,
            xrtValueString((xstrview){"home@example.com", 16})) ||
        !xrtValueArrayAppendNew(emails,
            xrtValueString((xstrview){"work@example.com", 16}))) goto done;
    {
        xvalue *owned = emails;
        emails = NULL;
        if (!xrtValueObjectSetNew(card, (xstrview){"email", 5}, owned)) goto done;
    }
    built = xv_build(card);
    if (built == NULL || strcmp(built, expected) != 0) goto done;
    back = xv_parse(built, strlen(built));
    if (back == NULL) goto done;
    name = xrtValueObjectGet(back, (xstrview){"fn", 2});
    if (name == NULL || !xrtValueGetString(name, &text) ||
        text.Size != strlen("王小明") ||
        memcmp(text.Data, "王小明", text.Size) != 0) goto done;
    {
        xvalue *read_emails = xrtValueObjectGet(back, (xstrview){"email", 5});
        xstrview first, second;
        if (read_emails == NULL || xrtValueCount(read_emails) != 2 ||
            !xrtValueGetString(xrtValueArrayAt(read_emails, 0), &first) ||
            !xrtValueGetString(xrtValueArrayAt(read_emails, 1), &second) ||
            first.Size != 16 || memcmp(first.Data, "home@example.com", 16) != 0 ||
            second.Size != 16 || memcmp(second.Data, "work@example.com", 16) != 0)
            goto done;
    }
    if (argc == 2 && !xrtFileWriteAll(argv[1],
        (xbytesview){(const unsigned char *)built, strlen(built)})) goto done;
    printf("name: %.*s\n", (int)text.Size, text.Data);
    printf("emails: %lld\n", (long long)xrtValueCount(
        xrtValueObjectGet(back, (xstrview){"email", 5})));
    printf("has CRLF: %s\n", strstr(built, "\r\n") != NULL ? "true" : "false");
    puts("vcard example: PASS");
    status = 0;
done:
    xrtValueRelease(back);
    xrtFree(built);
    xrtValueRelease(emails);
    xrtValueRelease(card);
    return status;
}

/* TCC 单文件范例包含实现；正式 C 构建仍分别编译三个单元。 */
#ifdef __TINYC__
#include "../vcard_native.c"
#include "../vcard_parse.c"
#include "../vcard_build.c"
#endif
