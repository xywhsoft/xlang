#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../op_native.c"

static int read_frame(op_context *context, HWND marker)
{
    int attempt;
    for (attempt = 0; attempt < 20; attempt++) {
        char *frame;
        SetWindowPos(marker, NULL, (attempt & 1) ? 0 : 16, 0, 8, 8,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        frame = xop_get_frame_info(context);
        if (frame && strcmp(frame, "0x0") != 0 && strchr(frame, 'x')) {
            xrtFree(frame);
            return 1;
        }
        xrtFree(frame);
        Sleep(100u);
    }
    return 0;
}

int main(void)
{
    op_context *first = xop_create();
    op_context *second = xop_create();
    HWND marker = CreateWindowExW(WS_EX_TOOLWINDOW, L"BUTTON", L"op DXGI contexts",
                                  WS_POPUP, 0, 0, 8, 8, NULL, NULL, NULL, NULL);
    long long desktop = (long long)(intptr_t)GetDesktopWindow();
    int ok = 0;
    int stage = 1;

    if (!first || !second || !marker) goto done;
    ShowWindow(marker, SW_SHOWNOACTIVATE);
    stage = 2;
    if (!xop_bind_window(first, desktop, "dxgi", "normal", "normal", 0) ||
        !xop_bind_window(second, desktop, "dxgi", "normal", "normal", 0)) goto done;
    stage = 3;
    if (!read_frame(first, marker)) goto done;
    stage = 4;
    if (!g_dxgi.last_frame || !op_dxgi_copy_cached(second, &g_dxgi) ||
        second->frame_buf == g_dxgi.last_frame ||
        second->frame_w != first->frame_w || second->frame_h != first->frame_h ||
        memcmp(second->frame_buf, first->frame_buf,
               (size_t)first->frame_w * (size_t)first->frame_h * 4u) != 0)
        goto done;
    stage = 5;
    {
        char *frame = xop_get_frame_info(second);
        if (!frame || strcmp(frame, "0x0") == 0) {
            xrtFree(frame);
            goto done;
        }
        xrtFree(frame);
    }
    stage = 6;
    if (!read_frame(second, marker)) goto done;
    stage = 7;
    if (!first->dxgi_attached || !second->dxgi_attached ||
        !g_dxgi.dup || g_dxgi_users != 2) goto done;

    xop_unbind(first);
    stage = 8;
    if (first->dxgi_attached || !second->dxgi_attached ||
        !g_dxgi.dup || g_dxgi_users != 1 || !read_frame(second, marker)) goto done;
    stage = 9;
    if (!xop_bind_window(first, desktop, "dxgi", "normal", "normal", 0) ||
        !read_frame(first, marker) || g_dxgi_users != 2) goto done;
    xop_destroy(second);
    second = NULL;
    stage = 10;
    if (!g_dxgi.dup || g_dxgi_users != 1 || !read_frame(first, marker)) goto done;
    ok = 1;

done:
    if (marker) DestroyWindow(marker);
    xop_destroy(first);
    xop_destroy(second);
    if (ok && (g_dxgi.dup || g_dxgi.device || g_dxgi.d3d11_module ||
               g_dxgi.last_frame || g_dxgi_users))
        ok = 0;
    if (!ok) {
        const xerror *error = xrtGetError();
        fprintf(stderr, "op DXGI contexts stage %d: %s\n", stage,
                error ? xrtErrorMessage(error) : "no error");
        return 1;
    }
    puts("op DXGI contexts: ok");
    return 0;
}
