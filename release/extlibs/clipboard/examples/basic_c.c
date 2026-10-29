#include "../include/clipboard.h"
#ifdef __TINYC__
/* 单文件 TCC 范例共用包内实现；正式包仍独立编译。 */
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shell32")
#include "../clipboard_native.c"
#endif
#include <stdio.h>
#include <string.h>

int main(void)
{
    xvalue* name = x_clipboard_format_name(13);
    xstrview text;
    if (!x_clipboard_available() || !name ||
        !xrtValueGetString(name, &text) ||
        text.Size != strlen("CF_UNICODETEXT") ||
        memcmp(text.Data, "CF_UNICODETEXT", text.Size) != 0) {
        xrtValueRelease(name);
        return 1;
    }
    puts("available=true");
    printf("format=%.*s\n", (int)text.Size, text.Data);
    xrtValueRelease(name);
    return 0;
}
