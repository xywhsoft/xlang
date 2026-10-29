#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../op_native.c"
#include "gdi_window_fixture.c"

int main(void)
{
    long long window = 0;
    op_context *ctx = NULL;
    char *color = NULL;
    BYTE *previous_frame;
    int ok = 0;
    int stage = 1;

    window = op_test_gdi_window_create();
    ctx = xop_create();
    if (!window || !ctx) goto done;
    stage = 2;
    if (!xop_bind_window(ctx, window,
                         "gdi", "normal", "normal", 0)) goto done;
    color = xop_get_color(ctx, 0, 0);
    if (!color || strcmp(color, "FF0000") != 0) goto done;
    xrtFree(color);
    color = NULL;
    stage = 3;
    previous_frame = ctx->frame_buf;
    if (op_ensure_frame(ctx, INT_MAX, INT_MAX) ||
        ctx->frame_buf != previous_frame || ctx->frame_w != 8 || ctx->frame_h != 8 ||
        ctx->frame_buf[2] != 255u) goto done;
    stage = 4;
    color = xop_get_color(ctx, 7, 7);
    if (!color || strcmp(color, "FF0000") != 0) goto done;
    xrtFree(color);
    color = NULL;
    stage = 5;
    if (!xop_bind_window(ctx, window, "opengl", "normal", "normal", 0)) goto done;
    color = xop_get_color(ctx, 0, 0);
    if (color != NULL || ctx->frame_buf || ctx->mem_dc || ctx->mem_bmp) goto done;
    ok = 1;

done:
    if (!ok) {
        const xerror *error = xrtGetError();
        fprintf(stderr, "op GDI frame stage %d: color=%s error=%s\n", stage,
                color ? color : "(null)",
                error ? xrtErrorMessage(error) : "no error");
    }
    xrtFree(color);
    xop_destroy(ctx);
    op_test_gdi_window_destroy(window);
    if (ok) puts("op GDI frame: ok");
    return ok ? 0 : 1;
}
