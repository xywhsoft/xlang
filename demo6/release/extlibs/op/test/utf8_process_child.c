#include <windows.h>
#include <wchar.h>

/* 与父进程使用同一组有界测试事件名。 */
static void event_name(WCHAR *output, const WCHAR *kind, DWORD parent)
{
    wsprintfW(output, L"Local\\op_utf8_%s_%lu", kind, (unsigned long)parent);
}

int main(void)
{
    WCHAR parent_text[32];
    WCHAR ready_name[96];
    WCHAR stop_name[96];
    WNDCLASSW window_class = {0};
    HMODULE module;
    HANDLE ready;
    HANDLE stop;
    HWND window;
    DWORD parent;

    if (!GetEnvironmentVariableW(L"OP_TEST_PARENT_PID", parent_text, 32)) return 1;
    parent = (DWORD)wcstoul(parent_text, NULL, 10);
    event_name(ready_name, L"ready", parent);
    event_name(stop_name, L"stop", parent);
    ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready_name);
    stop = OpenEventW(SYNCHRONIZE, FALSE, stop_name);
    if (!ready || !stop) return 2;

    module = LoadLibraryW(L"build/test/op/\x4E2D\x6587\x6A21\x5757.dll");
    if (!module) return 3;
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = L"op_utf8_process_window";
    if (!RegisterClassW(&window_class)) return 4;
    window = CreateWindowExW(0, window_class.lpszClassName,
                             L"\x4F60\x597D OP", WS_OVERLAPPED,
                             0, 0, 32, 32, NULL, NULL, window_class.hInstance, NULL);
    if (!window) return 5;

    SetEvent(ready);
    WaitForSingleObject(stop, 10000);
    DestroyWindow(window);
    FreeLibrary(module);
    CloseHandle(ready);
    CloseHandle(stop);
    return 0;
}
