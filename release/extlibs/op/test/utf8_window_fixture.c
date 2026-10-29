#include <windows.h>
#include <stdint.h>

static const WCHAR op_test_class_name[] = L"op_utf8_title_test";
static WCHAR op_test_received[64];
static int op_test_received_count;

static LRESULT CALLBACK op_test_window_proc(HWND window, UINT message,
                                             WPARAM word, LPARAM parameter)
{
    if (message == WM_CHAR && op_test_received_count < 64) {
        op_test_received[op_test_received_count++] = (WCHAR)word;
        return 0;
    }
    return DefWindowProcW(window, message, word, parameter);
}

int64_t op_test_create_utf8_window(void)
{
    WNDCLASSW window_class = {0};
    HWND window;

    op_test_received_count = 0;
    window_class.lpfnWndProc = op_test_window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = op_test_class_name;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    window = CreateWindowExW(0, op_test_class_name, L"initial",
                             WS_OVERLAPPED, 0, 0, 32, 32,
                             NULL, NULL, window_class.hInstance, NULL);
    return (int64_t)(intptr_t)window;
}

int64_t op_test_pump_utf8_window(int64_t handle)
{
    MSG message;
    while (PeekMessageW(&message, (HWND)(intptr_t)handle, 0, 0, PM_REMOVE))
        DispatchMessageW(&message);
    return op_test_received_count;
}

int64_t op_test_received_utf16_at(int64_t index)
{
    return index >= 0 && index < op_test_received_count ?
        op_test_received[index] : -1;
}
