/* C 核心写调用者缓冲，语言入口只负责转移独立字符串，不保留环形缓存。 */
#include "include/color_native.h"

char* xcolor_hex_text(int64_t r, int64_t g, int64_t b) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_hex(r, g, b, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_hex_a_text(int64_t r, int64_t g, int64_t b, int64_t a) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_hex_a(r, g, b, a, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_normalize_text(const char *input) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_normalize(input, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_from_hsv_text(double h, double s, double v) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_from_hsv(h, s, v, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_from_hsl_text(double h, double s, double l) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_from_hsl(h, s, l, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_mix_text(const char *a, const char *b, double t) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_mix(a, b, t, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_from_name_text(const char *name) {
    char text[XCOLOR_HEX_SIZE];
    (void)xcolor_from_name(name, text, sizeof(text));
    return xrtStrDup(text);
}

char* xcolor_to_name_text(const char *input) {
    char text[XCOLOR_NAME_SIZE];
    (void)xcolor_to_name(input, text, sizeof(text));
    return xrtStrDup(text);
}
