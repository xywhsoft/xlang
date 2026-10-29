#include <windows.h>
#include <stdint.h>

static LRESULT CALLBACK op_test_gdi_window_proc(HWND window, UINT message,
                                                 WPARAM wparam, LPARAM lparam)
{
    if (message == WM_PRINT || message == WM_PRINTCLIENT) {
        RECT rect = {0, 0, 8, 8};
        HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
        FillRect((HDC)wparam, &rect, red);
        DeleteObject(red);
        return 1;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT rect = {0, 0, 8, 8};
        HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
        FillRect(dc, &rect, red);
        DeleteObject(red);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

long long op_test_gdi_window_create(void)
{
    WNDCLASSW cls = {0};
    HWND window;
    cls.lpfnWndProc = op_test_gdi_window_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"op_gdi_frame_test";
    if (!RegisterClassW(&cls)) return 0;
    window = CreateWindowExW(WS_EX_TOOLWINDOW, cls.lpszClassName, L"",
                             WS_POPUP, -32000, -32000, 8, 8,
                             NULL, NULL, cls.hInstance, NULL);
    if (!window) {
        UnregisterClassW(cls.lpszClassName, cls.hInstance);
        return 0;
    }
    ShowWindow(window, SW_SHOWNOACTIVATE);
    UpdateWindow(window);
    return (long long)(intptr_t)window;
}

void op_test_gdi_window_destroy(long long handle)
{
    if (handle) DestroyWindow((HWND)(intptr_t)handle);
    UnregisterClassW(L"op_gdi_frame_test", GetModuleHandleW(NULL));
}
