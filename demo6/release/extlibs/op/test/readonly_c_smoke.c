#include <windows.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "../include/op.h"

static int text_matches(void *text, const char *expected)
{
    int matches = text && strcmp((const char *)text, expected) == 0;
    xrtFree(text);
    return matches;
}

static int text_has_length(void *text, size_t length)
{
    int matches = text && strlen((const char *)text) == length;
    xrtFree(text);
    return matches;
}

int main(void)
{
    void *op = xop_create();
    long long desktop = (long long)(intptr_t)GetDesktopWindow();
    char *screen = xop_get_screen_size();
    int valid_screen = screen && strchr(screen, 'x');
    xrtFree(screen);
    if (!op || !desktop || !valid_screen) return 1;
    if (xop_is_bind(op) || xop_get_bind_window(op)) return 2;
    if (!xop_bind_window(op, desktop, "normal", "normal", "normal", 0)) return 3;
    if (!xop_is_bind(op) || xop_get_bind_window(op) != desktop) return 4;
    void *color = xop_get_color(op, 0, 0);
    void *frame = xop_get_frame_info(op);
    int frame_ok = frame && strchr((const char *)frame, 'x') &&
                   strcmp((const char *)frame, "0x0") != 0;
    xrtFree(frame);
    int color_ok = color && strlen((const char *)color) == 6;
    xrtFree(color);
    if (!frame_ok || !color_ok) return 5;
    xrtClearError();
    color = xop_get_color(op, -1, -1);
    if (color != NULL || xrtGetError() == NULL) {
        xrtFree(color);
        return 20;
    }
    color = xop_get_color(op, 0, 0);
    if (!color || strlen((const char *)color) != 6u) {
        xrtFree(color);
        return 21;
    }
    xrtFree(color);
    if (!text_matches(xop_image_hash(op, LLONG_MAX, 0, LLONG_MAX - 1, 1), ""))
        return 7;
    if (!text_matches(xop_image_hash(op, LLONG_MAX - 1, 0, LLONG_MAX, 1), ""))
        return 8;
    if (!text_has_length(xop_image_hash(op, -10, -10, 1, 1), 16))
        return 9;
    if (xop_get_color_num(op, LLONG_MAX - 1, 0, LLONG_MAX, 1,
                          "FFFFFF", 1.0) != 0) return 10;
    if (!text_matches(xop_find_color(op, LLONG_MAX - 1, 0, LLONG_MAX, 1,
                                      "FFFFFF", 1.0, 0), "")) return 11;
    if (!text_matches(xop_get_colors(op, 10, 10, 9, 10), "")) return 12;
    if (!text_matches(xop_find_color_ex(op, 10, 10, 9, 10,
                                         "FFFFFF", 1.0, 0), "")) return 14;
    if (!text_matches(xop_find_color(op, 0, 0, 0, 0,
                                      "000000", 0.0, 0), "0,0")) return 16;
    if (!text_matches(xop_find_multi_color(op, 0, 0, 1, 0,
                                            "000000", "1|0|000000", 0.0, 0),
                      "0,0")) return 17;
    {
        char *colors = xop_get_colors(op, 0, 0, 99, 99);
        char *matches = xop_find_color_ex(op, 0, 0, 0, 0,
                                           "000000", 0.0, 0);
        int complete = colors && strlen(colors) == 69999 &&
                       matches && strcmp(matches, "0,0") == 0 &&
                       strlen(colors) == 69999;
        xrtFree(matches);
        xrtFree(colors);
        if (!complete) return 15;
    }
    {
        char *large = xop_capture_to_mem(op, 0, 0, 100, 100);
        char *clipped = xop_capture_to_mem(op, -1, -1, 2, 2);
        int complete = large && clipped &&
                       strlen(large) == 80008 &&
                       strncmp(large, "100x100|", 8) == 0 &&
                       strlen(clipped) == 36 &&
                       strncmp(clipped, "2x2|", 4) == 0;
        xrtFree(large);
        xrtFree(clipped);
        if (!complete) return 18;
        if (!text_matches(xop_capture_to_mem(op, LLONG_MAX - 1, 0,
                                              LLONG_MAX, 1), "")) return 19;
    }
    if (!text_matches(xop_find_color_block(op, 0, 0, 1, 1,
                                            "FFFFFF", 1.0, LLONG_MAX, 1), ""))
        return 13;
    if (!xop_unbind(op) || xop_is_bind(op)) return 6;
    xop_destroy(op);
    puts("op C read-only smoke: ok");
    return 0;
}
