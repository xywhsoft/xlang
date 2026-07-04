#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const char* text = "A\xE4\xBD\xA0\xE5\xA5\xBD";
    unsigned char invalid = 255u;
    size_t utf16Units = 0u;
    u16str utf16;
    u8str restored;
    u8str converted;

    xrtInit();

    require_line(
        "detect",
        xrtDetectCharset((void*)text, strlen(text), true) == XRT_CP_UTF8 &&
        !xrtIsUTF8((char*)&invalid, 1u) &&
        xrtDetectCharset(&invalid, 1u, true) == XRT_CP_OEM);

    utf16 = xrtUTF8to16((u8str)text, strlen(text), &utf16Units);
    restored = xrtUTF16to8(utf16, utf16Units, NULL);
    require_line(
        "utf16",
        utf16 != NULL &&
        utf16Units == 3u &&
        restored != NULL &&
        strcmp((const char*)restored, text) == 0);

    converted = (u8str)xrtConvCharset(utf16, utf16Units, XRT_CP_UTF16, XRT_CP_UTF8, NULL);
    require_line(
        "convert",
        converted != NULL &&
        strcmp((const char*)converted, text) == 0);

    xrtFree(converted);
    xrtFree(restored);
    xrtFree(utf16);
    xrtUnit();
    return 0;
}
