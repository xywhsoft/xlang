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

static int same(void* owned, const char* expected)
{
    int equal = owned != NULL && strcmp((const char*)owned, expected) == 0;
    xrtFree(owned);
    return equal;
}

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

    /* 比较原始字节，不依赖终端是否启用颜色或光标控制。 */
    if (!same(xansi_reset(), "\x1b[0m") ||
        !same(xansi_bold("x"), "\x1b[1mx\x1b[0m") ||
        !same(xansi_dim("x"), "\x1b[2mx\x1b[0m") ||
        !same(xansi_italic("x"), "\x1b[3mx\x1b[0m") ||
        !same(xansi_underline("x"), "\x1b[4mx\x1b[0m") ||
        !same(xansi_blink("x"), "\x1b[5mx\x1b[0m") ||
        !same(xansi_inverse("x"), "\x1b[7mx\x1b[0m") ||
        !same(xansi_hidden("x"), "\x1b[8mx\x1b[0m") ||
        !same(xansi_strike("x"), "\x1b[9mx\x1b[0m")) return 1;
    puts("styles: PASS");

    if (!same(xansi_fg("x", 31), "\x1b[31mx\x1b[0m") ||
        !same(xansi_bg("x", 41), "\x1b[41mx\x1b[0m") ||
        !same(xansi_fg256("x", 17), "\x1b[38;5;17mx\x1b[0m") ||
        !same(xansi_bg256("x", 17), "\x1b[48;5;17mx\x1b[0m") ||
        !same(xansi_fg_true("x", 1, 2, 3), "\x1b[38;2;1;2;3mx\x1b[0m") ||
        !same(xansi_bg_true("x", 1, 2, 3), "\x1b[48;2;1;2;3mx\x1b[0m")) return 1;
    puts("colors: PASS");

    if (!same(xansi_up(2), "\x1b[2A") ||
        !same(xansi_down(2), "\x1b[2B") ||
        !same(xansi_right(2), "\x1b[2C") ||
        !same(xansi_left(2), "\x1b[2D") ||
        !same(xansi_next_line(2), "\x1b[2E") ||
        !same(xansi_prev_line(2), "\x1b[2F") ||
        !same(xansi_column(2), "\x1b[2G")) return 1;
    puts("cursor controls: PASS");

    if (!same(xansi_clear_screen(), "\x1b[2J") ||
        !same(xansi_clear_line(), "\x1b[2K") ||
        !same(xansi_clear_to_end(), "\x1b[K") ||
        !same(xansi_hide_cursor(), "\x1b[?25l") ||
        !same(xansi_show_cursor(), "\x1b[?25h") ||
        !same(xansi_title("中文"), "\x1b]0;中文\a")) return 1;
    puts("screen controls: PASS");

    puts("ansi example: PASS");
    xrtFree(cursor);
    xrtFree(visible);
    xrtFree(styled);
    return 0;
}
