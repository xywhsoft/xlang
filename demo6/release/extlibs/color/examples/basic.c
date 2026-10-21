/* 与 examples/extlibs/color_basic.xl 对照；输出缓冲均属于调用者，无需堆分配。 */
#include "../include/color.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char accent[XCOLOR_HEX_SIZE], text[XCOLOR_NAME_SIZE], name[XCOLOR_NAME_SIZE];
    char black[XCOLOR_HEX_SIZE], white[XCOLOR_HEX_SIZE];
    const char* palette[] = {white, accent, black};

    /* 常用表示形式。 */
    if (!xcolor_normalize("#36c", accent, sizeof(accent)) ||
        !xcolor_valid(accent) || xcolor_alpha(accent) != 255 ||
        xcolor_red(accent) != 51 || xcolor_green(accent) != 102 ||
        xcolor_blue(accent) != 204 ||
        !xcolor_from_hsv(xcolor_hue(accent), xcolor_saturation(accent),
            xcolor_value(accent), text, sizeof(text)) ||
        strcmp(text, accent) != 0 || xcolor_lightness("#f00") != 0.5) return 1;
    printf("accent: %s\n", accent);
    if (!xcolor_hex_a(51, 102, 204, 128, text, sizeof(text))) return 1;
    printf("rgba: %s\n", text);
    if (!xcolor_from_hsl(120, 1, 0.5, text, sizeof(text))) return 1;
    printf("hsl: %s\n", text);
    if (!xcolor_from_name("RebeccaPurple", text, sizeof(text)) ||
        !xcolor_to_name("#663399", name, sizeof(name))) return 1;
    printf("named: %s / %s\n", text, name);

    /* 混色、对比度与无效输入。 */
    if (!xcolor_hex(0, 0, 0, black, sizeof(black)) ||
        !xcolor_hex(255, 255, 255, white, sizeof(white)) ||
        !xcolor_mix(black, white, 0.5, text, sizeof(text))) return 1;
    printf("middle: %s\n", text);
    for (size_t i = 0; i < 3; ++i) {
        double b = xcolor_contrast(palette[i], black);
        double w = xcolor_contrast(palette[i], white);
        const char* foreground = w > b ? white : black;
        double ratio = w > b ? w : b;
        if (ratio < 4.5 || xcolor_luminance(palette[i]) < 0 ||
            xcolor_is_dark(palette[i]) == xcolor_is_light(palette[i])) return 1;
        printf("%s -> %s %s\n", palette[i], foreground, ratio >= 7 ? "AAA" : "AA");
    }
    if (xcolor_valid("no-color") || xcolor_red("no-color") != -1 ||
        xcolor_hex(256, 0, 0, text, sizeof(text)) ||
        xcolor_from_name("unknown", text, sizeof(text)) ||
        xrtGetError() != NULL) return 1;
    puts("invalid: rejected");
    return 0;
}

/* 范例自包含 C 核心；直接 xl run 时无需额外指定源文件。 */
#include "../color_core.c"
