#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"

extern void *xop_create(void);
extern void xop_destroy(void *ptr);
extern long long xop_bind_window(void *ptr, long long hwnd, const char *display,
                                const char *mouse, const char *keypad, long long mode);
extern void *xop_get_frame_info(void *ptr);

int main(void)
{
    void *op = xop_create();
    char *frame = NULL;
    HWND marker = NULL;
    int attempt;
    if (!op) return 1;
    if (!xop_bind_window(op, (long long)(intptr_t)GetDesktopWindow(),
                         "dxgi", "normal", "normal", 0)) {
        xop_destroy(op);
        return 2;
    }
    /* Desktop Duplication 只报告新帧：测试窗口触发一次真实桌面更新。 */
    marker = CreateWindowExW(WS_EX_TOOLWINDOW, L"BUTTON", L"op DXGI test", WS_POPUP,
                             0, 0, 8, 8, NULL, NULL, NULL, NULL);
    if (marker) {
        ShowWindow(marker, SW_SHOWNOACTIVATE);
        UpdateWindow(marker);
    }
    for (attempt = 0; attempt < 20; ++attempt) {
        if (marker) SetWindowPos(marker, NULL, (attempt & 1) ? 0 : 16, 0, 8, 8,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
        xrtFree(frame);
        frame = (char *)xop_get_frame_info(op);
        if (frame && strcmp(frame, "0x0") != 0) break;
        Sleep(100u);
    }
    if (marker) DestroyWindow(marker);
    if (!frame || strcmp(frame, "0x0") == 0 || !strchr(frame, 'x')) {
        const xerror *error = xrtGetError();
        fprintf(stderr, "DXGI frame unavailable: %s (%s)\n",
                frame ? frame : "(null)", error ? xrtErrorMessage(error) : "no error");
        xrtFree(frame);
        xop_destroy(op);
        return 3;
    }
    printf("op DXGI desktop frame: %s\n", frame);
    xrtFree(frame);
    xop_destroy(op);
    return 0;
}
