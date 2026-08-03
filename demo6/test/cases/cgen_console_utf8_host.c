#include "src/common/xl6_console.h"
#include "lib/xrt/xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static SHORT testCursorX = 0;
static WCHAR testActualText[4] = { 0, 0, 0, 0 };
static DWORD testActualSize = 0;
static WCHAR testDirectText[4] = { 0, 0, 0, 0 };
static DWORD testDirectSize = 0;

static int testConsoleSessionAndWrite(void)
{
    static const char utf8Text[] = "\xE4\xB8\xAD\xE6\x96\x87";
    HANDLE originalOutput;
    HANDLE originalError;
    HANDLE screen;
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    COORD origin;
    UINT originalOutputCP;
    int outputReplaced;
    int errorReplaced;
    int allocatedConsole;
    int result;

    originalOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    originalError = GetStdHandle(STD_ERROR_HANDLE);
    outputReplaced = 0;
    errorReplaced = 0;
    allocatedConsole = 0;
    result = 1;

    if ( GetConsoleOutputCP() == 0 ) {
        if ( !AllocConsole() ) {
            return 1;
        }
        allocatedConsole = 1;
    }

    originalOutputCP = GetConsoleOutputCP();
    if ( originalOutputCP == 0 || !SetConsoleOutputCP(437u) ) {
        result = 2;
        goto cleanup;
    }

    screen = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);
    if ( screen == INVALID_HANDLE_VALUE ) {
        result = 3;
        goto cleanup;
    }

    if ( !SetStdHandle(STD_OUTPUT_HANDLE, screen) ) {
        result = 4;
        goto close_screen;
    }
    outputReplaced = 1;
    if ( !SetStdHandle(STD_ERROR_HANDLE, screen) ) {
        result = 5;
        goto restore_handles;
    }
    errorReplaced = 1;

    /*
     * XRT 对真实控制台直接调用 WriteConsoleW，对重定向句柄写 UTF-8。
     * 会话边界不得修改进程共享代码页，否则嵌入宿主和并行上下文会互相干扰。
     */
    xl6ConsoleSessionBegin();
    if ( GetConsoleOutputCP() != 437u ) {
        result = 6;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }

    xl6ConsoleSessionBegin();
    xl6ConsoleSessionEnd();
    if ( GetConsoleOutputCP() != 437u ) {
        result = 7;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }

    {
        xvalue value = xvoCreateText(
            (void*)utf8Text,
            (uint32)(sizeof(utf8Text) - 1u),
            false);
        if ( value == NULL ) {
            result = 8;
            xl6ConsoleSessionEnd();
            goto restore_handles;
        }
        xvoPrint(value, false);
        xvoUnref(value);
    }

    if ( !GetConsoleScreenBufferInfo(screen, &screenInfo) ) {
        result = 15;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }
    testCursorX = screenInfo.dwCursorPosition.X;

    origin.X = 0;
    origin.Y = 0;
    testActualSize = 0;
    if ( !ReadConsoleOutputCharacterW(
            screen,
            testActualText,
            4u,
            origin,
            &testActualSize) ||
         testActualSize < 2u ||
         testActualText[0] != 0x4E2D ||
         testActualText[1] != 0x6587 ) {
        result = 9;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }

    if ( !SetConsoleCursorPosition(screen, origin) ) {
        result = 10;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }
    if ( !xl6ConsoleWrite(XL6_CONSOLE_STDOUT, utf8Text, sizeof(utf8Text) - 1u) ) {
        result = 11;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }

    if ( !GetConsoleScreenBufferInfo(screen, &screenInfo) ) {
        result = 12;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }
    origin.X = 0;
    origin.Y = screenInfo.dwCursorPosition.Y;
    if ( origin.X == screenInfo.dwCursorPosition.X ) {
        origin.Y = (SHORT)(origin.Y - 1);
    }
    if ( origin.Y < 0 ) {
        origin.Y = 0;
    }

    testDirectSize = 0;
    if ( !ReadConsoleOutputCharacterW(
            screen,
            testDirectText,
            4u,
            origin,
            &testDirectSize) ||
         testDirectSize < 2u ||
         testDirectText[0] != 0x4E2D ||
         testDirectText[1] != 0x6587 ) {
        result = 13;
        xl6ConsoleSessionEnd();
        goto restore_handles;
    }

    xl6ConsoleSessionEnd();
    if ( GetConsoleOutputCP() != 437u ) {
        result = 14;
        goto restore_handles;
    }
    result = 0;

restore_handles:
    if ( outputReplaced ) {
        (void)SetStdHandle(STD_OUTPUT_HANDLE, originalOutput);
    }
    if ( errorReplaced ) {
        (void)SetStdHandle(STD_ERROR_HANDLE, originalError);
    }

close_screen:
    CloseHandle(screen);

cleanup:
    if ( originalOutputCP != 0 ) {
        (void)SetConsoleOutputCP(originalOutputCP);
    }
    if ( allocatedConsole ) {
        (void)FreeConsole();
    }
    return result;
}
#endif

int main(void)
{
    int result;

    if ( xrtInit() == NULL ) {
        return 20;
    }
#if defined(_WIN32) || defined(_WIN64)
    result = testConsoleSessionAndWrite();
    if ( result != 0 ) {
        fprintf(
            stderr,
            "console UTF-8 host stage=%d cursor=%d text=%04X,%04X,%04X,%04X size=%lu direct=%04X,%04X,%04X,%04X size=%lu\n",
            result,
            (int)testCursorX,
            (unsigned int)testActualText[0],
            (unsigned int)testActualText[1],
            (unsigned int)testActualText[2],
            (unsigned int)testActualText[3],
            (unsigned long)testActualSize,
            (unsigned int)testDirectText[0],
            (unsigned int)testDirectText[1],
            (unsigned int)testDirectText[2],
            (unsigned int)testDirectText[3],
            (unsigned long)testDirectSize);
    }
#else
    xl6ConsoleSessionBegin();
    xl6ConsoleSessionEnd();
    result = 0;
#endif
    xrtUnit();
    return result;
}
