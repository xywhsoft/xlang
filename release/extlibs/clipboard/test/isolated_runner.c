#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int wide_from_utf8(const char* text, wchar_t* out, int capacity)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               text, -1, out, capacity) > 0;
}

static int put_text(const wchar_t* text)
{
    SIZE_T bytes = (wcslen(text) + 1u) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!data) return 0;
    void* memory = GlobalLock(data);
    if (!memory) { GlobalFree(data); return 0; }
    memcpy(memory, text, bytes);
    GlobalUnlock(data);
    if (!OpenClipboard(NULL)) { GlobalFree(data); return 0; }
    int ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
    if (!ok) GlobalFree(data);
    CloseClipboard();
    return ok;
}

static int is_text(const wchar_t* expected)
{
    if (!OpenClipboard(NULL)) return 0;
    HGLOBAL data = (HGLOBAL)GetClipboardData(CF_UNICODETEXT);
    const wchar_t* text = data ? (const wchar_t*)GlobalLock(data) : NULL;
    int equal = text && wcscmp(text, expected) == 0;
    if (text) GlobalUnlock(data);
    CloseClipboard();
    return equal;
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 5) return 2;
    int binary = argc == 5 && strcmp(argv[4], "--binary") == 0;
    int restored = argc == 5 && strcmp(argv[4], "--restored") == 0;
    if (argc == 5 && !binary && !restored) return 2;
    wchar_t exe[512], script[256], directory[512];
    if (!wide_from_utf8(argv[1], exe, 512) ||
        !wide_from_utf8(argv[2], script, 256) ||
        !wide_from_utf8(argv[3], directory, 512)) return 2;

    /* 每个测试进程独占一个 window station 和剪贴板。 */
    HWINSTA station = CreateWindowStationW(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    if (!station || !SetProcessWindowStation(station)) return 3;
    HDESK desktop = CreateDesktopW(L"clipboard-test", NULL, NULL, 0,
                                    GENERIC_ALL, NULL);
    if (!desktop || !SetThreadDesktop(desktop)) return 3;
    wchar_t station_name[128];
    DWORD needed = 0;
    if (!GetUserObjectInformationW(station, UOI_NAME, station_name,
                                   sizeof(station_name), &needed) ||
        wcslen(station_name) + wcslen(L"\\clipboard-test") + 1u > 128u)
        return 3;
    wcscat(station_name, L"\\clipboard-test");
    if (!put_text(L"before")) return 4;

    wchar_t command[1024];
    int command_size = binary
        ? swprintf(command, 1024, L"\"%ls\"", exe)
        : swprintf(command, 1024, L"\"%ls\" run %ls", exe, script);
    if (command_size < 0 || command_size >= 1024)
        return 5;
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE output_read = NULL, output_write = NULL;
    if (!CreatePipe(&output_read, &output_write, &security, 0)) return 5;
    if (!SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(output_read);
        CloseHandle(output_write);
        return 5;
    }
    startup.cb = sizeof(startup);
    startup.lpDesktop = station_name;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output_write;
    startup.hStdError = output_write;
    if (!CreateProcessW(exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, directory, &startup, &process)) {
        fprintf(stderr, "CreateProcessW: %lu\n", (unsigned long)GetLastError());
        CloseHandle(output_read);
        CloseHandle(output_write);
        return 5;
    }
    CloseHandle(output_write);
    DWORD waited = WaitForSingleObject(process.hProcess, 30000);
    DWORD exit_code = 1;
    if (waited == WAIT_TIMEOUT) TerminateProcess(process.hProcess, 1);
    if (waited == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (waited != WAIT_OBJECT_0 || exit_code != 0 ||
        !is_text(restored ? L"before" : L"after")) {
        char output[1024];
        DWORD count = 0, available = 0;
        while (PeekNamedPipe(output_read, NULL, 0, NULL, &available, NULL) && available) {
            DWORD wanted = available < sizeof(output) ? available : sizeof(output);
            if (!ReadFile(output_read, output, wanted, &count, NULL) || !count) break;
            fwrite(output, 1, count, stderr);
        }
        fprintf(stderr, "isolated XLang clipboard test: exit=%lu wait=%lu\n",
                (unsigned long)exit_code, (unsigned long)waited);
        CloseHandle(output_read);
        return 1;
    }
    CloseHandle(output_read);
    puts("clipboard isolated XLang: PASS");
    return 0;
}
