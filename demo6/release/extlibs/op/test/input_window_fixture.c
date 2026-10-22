#include <windows.h>
#include <stdint.h>

int64_t op_test_create_input_window(void)
{
    HWND window = CreateWindowExW(0, L"STATIC", L"op input test", WS_POPUP,
                                   0, 0, 32, 32, NULL, NULL,
                                   GetModuleHandleW(NULL), NULL);
    return (int64_t)(intptr_t)window;
}

int64_t op_test_destroy_input_window(int64_t handle)
{
    return DestroyWindow((HWND)(intptr_t)handle) ? 1 : 0;
}
