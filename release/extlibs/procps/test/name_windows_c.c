#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static FARPROC WINAPI fake_resolve(HMODULE module, LPCSTR name);
static BOOL WINAPI fake_close(HANDLE handle);

/* 只替换本测试翻译单元的快照入口；Unicode 转换仍用真实实现。 */
#define GetProcAddress fake_resolve
#define CloseHandle fake_close
#include "../procps_native.c"
#undef CloseHandle
#undef GetProcAddress

static const WCHAR* input;
static unsigned closes, snapshots, cases, failures;

static HANDLE WINAPI fake_snapshot(DWORD flags, DWORD pid)
{
    (void)flags; (void)pid;
    ++snapshots;
    return (HANDLE)(uintptr_t)99;
}

static BOOL WINAPI fake_first(HANDLE snapshot, xprocps_entry* entry)
{
    (void)snapshot;
    entry->pid = 73;
    memcpy(entry->name, input, (wcslen(input) + 1u) * sizeof(WCHAR));
    return TRUE;
}

static BOOL WINAPI fake_next(HANDLE snapshot, xprocps_entry* entry)
{
    (void)snapshot; (void)entry;
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

static BOOL WINAPI fake_close(HANDLE handle)
{
    if (handle != (HANDLE)(uintptr_t)99) return FALSE;
    ++closes;
    return TRUE;
}

static FARPROC WINAPI fake_resolve(HMODULE module, LPCSTR name)
{
    FARPROC address = NULL;
    (void)module;
    if (strcmp(name, "CreateToolhelp32Snapshot") == 0) {
        xprocps_snapshot_fn function = fake_snapshot;
        memcpy(&address, &function, sizeof(address));
    } else if (strcmp(name, "Process32FirstW") == 0) {
        xprocps_first_fn function = fake_first;
        memcpy(&address, &function, sizeof(address));
    } else if (strcmp(name, "Process32NextW") == 0) {
        xprocps_next_fn function = fake_next;
        memcpy(&address, &function, sizeof(address));
    }
    return address;
}

static void check(bool ok, const char* label)
{
    ++cases;
    if (!ok) { ++failures; fprintf(stderr, "procps UTF16 case %u: %s\n", cases, label); }
}

int main(void)
{
    static const WCHAR high[] = {0xd800, 0}, low[] = {0xdc00, 0}, broken[] = {0xd800, 'A', 0};
    static const WCHAR unicode[] = {0x8fdb, 0x7a0b, '-', 0xd83d, 0xde00, '.', 'e', 'x', 'e', 0};
    WCHAR longest[MAX_PATH];
    char expected[MAX_PATH];
    static const struct { const WCHAR* text; const char* expected; } names[] = {
        {L"worker.exe", "worker.exe"}, {L"", ""},
        {unicode, "\xe8\xbf\x9b\xe7\xa8\x8b-\xf0\x9f\x98\x80.exe"},
        {high, NULL}, {low, NULL}, {broken, NULL}
    };
    xmemdebugsnapshot before, after;
    xrtSetErrorInfo(XERR_IO, "procps.test.old", 0, "warm diagnostic slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        input = names[i].text;
        xrtSetErrorInfo(XERR_IO, "procps.test.old", 0, "old diagnostic");
        char* result = xprocps_name(73);
        check(names[i].expected ? result && strcmp(result, names[i].expected) == 0 && !xrtGetError() :
            !result && xrtErrorKind(xrtGetError()) == XERR_VALUE, "UTF16 result and diagnostic");
        xrtFree(result);
        xrtClearError();
    }
    for (unsigned i = 0; i < MAX_PATH - 1u; ++i) { longest[i] = 'x'; expected[i] = 'x'; }
    longest[MAX_PATH - 1] = 0; expected[MAX_PATH - 1] = 0;
    input = longest;
    char* result = xprocps_name(73);
    check(result && strcmp(result, expected) == 0 && !xrtGetError(), "complete bounded snapshot name");
    xrtFree(result);
    input = L"worker.exe";
    for (unsigned budget = 0; budget < 2; ++budget) {
        if (!xrtMemDebugFailAfter(budget)) return 2;
        result = xprocps_name(73);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        check(budget == 0 ? !result && injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            result && !injected && strcmp(result, "worker.exe") == 0 && !xrtGetError(), "owned conversion budget");
        xrtFree(result);
        xrtClearError();
    }
    check(closes == snapshots && snapshots == 9, "snapshot closed once per call");
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount, "allocation counters");
    printf("procps Windows names: %u checks, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
