#include <limits.h>
#include <stdio.h>

#include "../op_native.c"

int main(void)
{
    op_context context = {0};
    int x1, y1, x2, y2;

    context.frame_w = 100;
    context.frame_h = 50;
    if (!op_clip_search_rect(&context, LLONG_MIN, LLONG_MIN,
                             LLONG_MAX, LLONG_MAX, &x1, &y1, &x2, &y2) ||
        x1 != 0 || y1 != 0 || x2 != 99 || y2 != 49) return 1;
    if (!op_clip_search_rect(&context, 99, 49, 99, 49,
                             &x1, &y1, &x2, &y2) ||
        x1 != 99 || y1 != 49 || x2 != 99 || y2 != 49) return 2;
    if (op_clip_search_rect(&context, LLONG_MAX - 1, 0,
                            LLONG_MAX, 10, &x1, &y1, &x2, &y2)) return 3;
    if (op_clip_search_rect(&context, -10, -10, -1, 10,
                            &x1, &y1, &x2, &y2)) return 4;
    if (op_clip_search_rect(&context, 10, 10, 9, 20,
                            &x1, &y1, &x2, &y2)) return 5;
    puts("op search region bounds: ok");
    return 0;
}
