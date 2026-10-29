/* Run language terminal tests in a real but invisible, private Windows console.
 * The user's terminal and input queue are never touched. */
#include <windows.h>
#include <wchar.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void fail(const char* expression, int line)
{
    DWORD error = GetLastError(), read = 0;
    WCHAR screen[8192]; char utf8[24576];
    FILE* diagnostic = fopen("build/test/stdlib_console_windows_host.failure.txt", "w");
    if (diagnostic != NULL) {
        fprintf(diagnostic, "line %d: %s (Win32 error %lu)\n", line, expression, (unsigned long)error);
        if (ReadConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE), screen, 8192, (COORD){0,0}, &read)) {
            int size = WideCharToMultiByte(CP_UTF8, 0, screen, (int)read, utf8, sizeof(utf8), NULL, NULL);
            if (size > 0) fwrite(utf8, 1, (size_t)size, diagnostic);
        }
        fclose(diagnostic);
    }
    ExitProcess(3);
}
#undef assert
#define assert(condition) do { if (!(condition)) fail(#condition, __LINE__); } while (0)

static void appendChar(WCHAR* command, size_t* length, WCHAR ch)
{
    assert(*length < 32766);
    command[(*length)++] = ch;
    command[*length] = 0;
}
static void appendQuoted(WCHAR* command, size_t* length, const WCHAR* arg)
{
    size_t slashes = 0;
    if (*length != 0) appendChar(command, length, ' ');
    appendChar(command, length, '"');
    for (;;) {
        WCHAR ch = *arg++;
        if (ch == '\\') { ++slashes; continue; }
        for (size_t i = 0; i < slashes * (ch == '"' || ch == 0 ? 2u : 1u); ++i)
            appendChar(command, length, '\\');
        slashes = 0;
        if (ch == 0) break;
        if (ch == '"') appendChar(command, length, '\\');
        appendChar(command, length, ch);
    }
    appendChar(command, length, '"');
}
static DWORD launch(WCHAR* command, DWORD flags)
{
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD code;
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    assert(CreateProcessW(NULL, command, NULL, NULL, FALSE, flags, NULL, NULL, &startup, &process));
    if (WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, 5000);
        assert(!"terminal child timed out");
    }
    assert(GetExitCodeProcess(process.hProcess, &code));
    assert(CloseHandle(process.hThread) && CloseHandle(process.hProcess));
    return code;
}
int wmain(int argc, WCHAR** argv)
{
    WCHAR command[32768] = {0};
    size_t length = 0;
    if (argc < 3) return 2;
    if (wcscmp(argv[1], L"--child") != 0) {
        WCHAR path[32768];
        assert(GetModuleFileNameW(NULL, path, 32768) != 0);
        appendQuoted(command, &length, path);
        appendQuoted(command, &length, L"--child");
        for (int i = 1; i < argc; ++i) appendQuoted(command, &length, argv[i]);
        return (int)launch(command, CREATE_NO_WINDOW);
    }
    assert(argc >= 4);
    {
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE), output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD beforeIn, beforeOut, afterIn, afterOut, written;
        CONSOLE_CURSOR_INFO beforeCursor, afterCursor;
        INPUT_RECORD records[2] = {{0}};
        assert(GetConsoleMode(input, &beforeIn) && GetConsoleMode(output, &beforeOut));
        assert(GetConsoleCursorInfo(output, &beforeCursor) && FlushConsoleInputBuffer(input));
        records[0].EventType = KEY_EVENT;
        records[0].Event.KeyEvent.bKeyDown = TRUE;
        records[0].Event.KeyEvent.wRepeatCount = 1;
        records[0].Event.KeyEvent.uChar.UnicodeChar = 0x4f60;
        records[1] = records[0];
        records[1].Event.KeyEvent.uChar.UnicodeChar = 0;
        records[1].Event.KeyEvent.wVirtualKeyCode = VK_UP;
        assert(WriteConsoleInputW(input, records, 2, &written) && written == 2);
        for (int i = 3; i < argc; ++i) appendQuoted(command, &length, argv[i]);
        assert(launch(command, 0) == (DWORD)wcstoul(argv[2], NULL, 10));
        assert(GetConsoleMode(input, &afterIn) && GetConsoleMode(output, &afterOut));
        assert(beforeIn == afterIn && beforeOut == afterOut);
        assert(GetConsoleCursorInfo(output, &afterCursor));
        assert(beforeCursor.bVisible == afterCursor.bVisible && beforeCursor.dwSize == afterCursor.dwSize);
    }
    return 0;
}
