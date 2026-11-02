/* 模拟系统文本，不访问真实注册表；转换、分配与错误槽仍使用真实XRT。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NUMBER_INTEGER
#include "xrt.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const WCHAR* input;
static size_t units;
static unsigned opened, closed, checks, failures, cases, budgets, injected;

static BOOL WINAPI fake_hostname(LPWSTR text, LPDWORD size);
static LONG WINAPI fake_open(HKEY root, LPCWSTR path, DWORD options,
    REGSAM access, PHKEY key);
static LONG WINAPI fake_query(HKEY key, LPCWSTR name, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD size);
static LONG WINAPI fake_close(HKEY key);

#define GetComputerNameW fake_hostname
#define RegOpenKeyExW fake_open
#define RegQueryValueExW fake_query
#define RegCloseKey fake_close
#include "../sysinfo_native.c"
#undef GetComputerNameW
#undef RegOpenKeyExW
#undef RegQueryValueExW
#undef RegCloseKey

static BOOL WINAPI fake_hostname(LPWSTR text, LPDWORD size)
{
    if (*size <= units) return FALSE;
    memcpy(text, input, units * sizeof(*text));
    text[units] = 0;
    *size = (DWORD)units;
    return TRUE;
}

static LONG WINAPI fake_open(HKEY root, LPCWSTR path, DWORD options,
    REGSAM access, PHKEY key)
{
    if (root != HKEY_LOCAL_MACHINE || options != 0 || access != KEY_READ ||
        wcscmp(path, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0") != 0)
        return ERROR_BAD_PATHNAME;
    ++opened;
    *key = (HKEY)(uintptr_t)71;
    return ERROR_SUCCESS;
}

static LONG WINAPI fake_query(HKEY key, LPCWSTR name, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD size)
{
    if (key != (HKEY)(uintptr_t)71 || reserved != NULL ||
        wcscmp(name, L"ProcessorNameString") != 0) return ERROR_BAD_PATHNAME;
    if (*size < units * sizeof(WCHAR)) return ERROR_MORE_DATA;
    memcpy(data, input, units * sizeof(WCHAR));
    *size = (DWORD)(units * sizeof(WCHAR));
    *type = REG_SZ;
    return ERROR_SUCCESS;
}

static LONG WINAPI fake_close(HKEY key)
{
    if (key != (HKEY)(uintptr_t)71) return ERROR_INVALID_HANDLE;
    ++closed;
    return ERROR_SUCCESS;
}

static void check(bool valid, const char* message)
{
    ++checks;
    if (!valid) {
        if (failures < 12) fprintf(stderr, "sysinfo Unicode: %s\n", message);
        ++failures;
    }
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount &&
        before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static void one(char* (*query)(void), const WCHAR* text, size_t length,
    const char* expected)
{
    xmemdebugsnapshot before;
    char* result;
    input = text;
    units = length;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xrtSetErrorInfo(XERR_IO, "sysinfo.test.old", 9, "old diagnostic");
    result = query();
    if (result && strcmp(result, expected) != 0 && failures < 12) {
        fprintf(stderr, "sysinfo Unicode unexpected bytes:");
        for (size_t i = 0; result[i]; ++i) fprintf(stderr, " %02x", (unsigned char)result[i]);
        fputc('\n', stderr);
    }
    check(result && !xrtGetError() && strcmp(result, expected) == 0,
        "complete text or documented unknown; old error cleared");
    xrtFree(result);
    check(balanced(&before), "normal query balances all seven memory metrics");

    /* 从实际首个分配点扫到无故障终点，不固定跨平台分配次数。 */
    bool completed = false;
    for (unsigned budget = 0; budget < 16; ++budget) {
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "arm real allocation failure");
        result = query();
        bool hit = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        ++budgets;
        if (hit) {
            ++injected;
            check(!result && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "allocation error must not become unknown");
        } else {
            completed = true;
            check(result && !xrtGetError() && strcmp(result, expected) == 0,
                "allocation scan reaches the correct result");
        }
        xrtFree(result);
        check(balanced(&before), "partial query releases all owners");
        result = query();
        check(result && !xrtGetError() && strcmp(result, expected) == 0,
            "same query recovers immediately");
        xrtFree(result);
        check(balanced(&before), "recovery has no retained allocation");
        if (completed) break;
    }
    check(completed, "bounded allocation scan completed");
    ++cases;
}

int main(int argc, char** argv)
{
    (void)argv;
    if (argc != 1 || !xrtMemDebugEnabled()) return 2;
    static const WCHAR high[] = {0xd800, 0}, low[] = {0xdc00, 0};
    static const WCHAR broken[] = {'A', 0xd800, 'B', 0};
    static const WCHAR swapped[] = {0xdc00, 0xd800, 0};
    static const WCHAR doubled[] = {0xd800, 0xd801, 0};
    static const WCHAR minimum[] = {0xd800, 0xdc00, 0};
    static const WCHAR maximum[] = {0xdbff, 0xdfff, 0};
    static const WCHAR chinese[] = {0x4e3b, 0x673a, 0};
    static const WCHAR emoji[] = {'A', 0xd83d, 0xde00, 'B', 0};
    static const WCHAR before_pair[] = {0xd7ff, 0}, after_pair[] = {0xe000, 0};
    static const WCHAR noncharacter[] = {0xffff, 0}, bom[] = {0xfeff, 0};
    static const struct { const WCHAR* text; const char* expected; } texts[] = {
        {L"worker", "worker"}, {L"", "unknown"},
        {chinese, "\xe4\xb8\xbb\xe6\x9c\xba"},
        {emoji, "A\xf0\x9f\x98\x80" "B"},
        {minimum, "\xf0\x90\x80\x80"}, {maximum, "\xf4\x8f\xbf\xbf"},
        {before_pair, "\xed\x9f\xbf"}, {after_pair, "\xee\x80\x80"},
        {noncharacter, "\xef\xbf\xbf"}, {bom, "\xef\xbb\xbf"},
        {high, "unknown"}, {low, "unknown"}, {broken, "unknown"},
        {swapped, "unknown"}, {doubled, "unknown"}
    };
    char* warm = xrtUtf16To8((const uint16*)high, NULL);
    xrtFree(warm);
    xrtClearError();
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i) {
        size_t length = wcslen(texts[i].text);
        one(xsysinfo_hostname, texts[i].text, length, texts[i].expected);
        one(xsysinfo_cpu_name, texts[i].text, length, texts[i].expected);
    }

    /* API容量的末端也要完整返回，不能丢失最后一个代理对。 */
    WCHAR host[MAX_COMPUTERNAME_LENGTH + 1], cpu[256];
    char host_expected[MAX_COMPUTERNAME_LENGTH + 1], cpu_expected[256];
    for (unsigned i = 0; i < MAX_COMPUTERNAME_LENGTH; ++i) host[i] = host_expected[i] = 'h';
    host[MAX_COMPUTERNAME_LENGTH] = host_expected[MAX_COMPUTERNAME_LENGTH] = 0;
    one(xsysinfo_hostname, host, MAX_COMPUTERNAME_LENGTH, host_expected);
    for (unsigned i = 0; i < 255; ++i) cpu[i] = cpu_expected[i] = 'c';
    cpu[255] = cpu_expected[255] = 0;
    one(xsysinfo_cpu_name, cpu, 255, cpu_expected);
    cpu[253] = 0xd83d; cpu[254] = 0xde00;
    char final_pair[258];
    memset(final_pair, 'c', 253);
    memcpy(final_pair + 253, "\xf0\x9f\x98\x80", 5);
    one(xsysinfo_cpu_name, cpu, 255, final_pair);
    cpu[254] = 0xd800;
    one(xsysinfo_cpu_name, cpu, 255, "unknown");
    host[MAX_COMPUTERNAME_LENGTH - 1] = 0xd800;
    one(xsysinfo_hostname, host, MAX_COMPUTERNAME_LENGTH, "unknown");
    one(xsysinfo_hostname, host, MAX_COMPUTERNAME_LENGTH + 1u, "unknown");
    one(xsysinfo_cpu_name, cpu, 256u, "unknown");
    one(xsysinfo_cpu_name, L" \t", 2, "unknown");
    one(xsysinfo_hostname, L" \t", 2, " \t");
    check(opened == closed, "registry handle closed exactly once per open");
    printf("sysinfo Windows Unicode: %u cases, %u checks, %u actual OOM/%u budgets, %u failures\n",
        cases, checks, injected, budgets, failures);
    return failures ? 1 : 0;
}
