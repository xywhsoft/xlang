#include "../include/color.h"
#include <stdio.h>

/* 与 theme.xl 同样选择背景、文字与悬停色；只使用调用者栈缓冲。 */
static bool theme(const char* name, const char* input)
{
    char background[XCOLOR_HEX_SIZE], hover[XCOLOR_HEX_SIZE];
    if (!xcolor_normalize(input, background, sizeof(background)) &&
        !xcolor_from_name(input, background, sizeof(background))) return false;
    const char* foreground = xcolor_contrast(background, "#FFFFFF") >
        xcolor_contrast(background, "#000000") ? "#FFFFFF" : "#000000";
    if (xcolor_contrast(background, foreground) < 4.5 ||
        !xcolor_mix(background, foreground, 0.15, hover, sizeof(hover))) return false;
    printf(".%s { --background: %s; --foreground: %s; --hover: %s; }\n",
        name, background, foreground, hover);
    return true;
}

int main(void)
{
    return theme("ocean", "#36c") && theme("plum", "RebeccaPurple") &&
        theme("paper", "#fff") ? 0 : 1;
}

/* 单文件直跑；名称不与 theme.xl 构成同名 C companion。 */
#include "../color_core.c"
