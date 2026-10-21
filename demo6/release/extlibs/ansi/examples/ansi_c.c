#ifdef __TINYC__
#include "../ansi_native.c"
#else
#define XRT_MODULE_STRING
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/ansi_native.h"
#endif
#include <stdio.h>
#include <string.h>

int main(void)
{
    char* styled = (char*)xansi_fg("中文", 32);
    char* visible;
    char* cursor;
    int64_t width;
    if (styled == NULL) return 1;
    visible = (char*)xansi_strip(styled);
    width = xansi_display_width(styled);
    cursor = (char*)xansi_up(2);
    if (visible == NULL || width < 0 || cursor == NULL) {
        xrtFree(cursor);
        xrtFree(visible);
        xrtFree(styled);
        return 1;
    }
    printf("visible:  %s\n", visible);
    printf("width:  %lld\n", (long long)width);
    printf("cursor:  %s\n", strcmp(cursor, "\x1b[2A") == 0 ? "true" : "false");
    puts("ansi example: PASS");
    xrtFree(cursor);
    xrtFree(visible);
    xrtFree(styled);
    return 0;
}
