#include "../include/xrt_shellwords.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const char *words[] = {
    "", "plain", "two words", "tab\tword", "line\nword", "cr\rword",
    "vertical\vtab", "form\ffeed",
    "a b\\", "a\\\"b",
    "a\\\\\"b", "\\\\", "\\\"", "trailing \\\\",
    "\xe4\xbd\xa0\xe5\xa5\xbd", "\xe4\xb8\xad \xe6\x96\x87",
    "\xe4\xbd\xa0\\\"\xe5\xa5\xbd", "emoji \xf0\x9f\x98\x80"
};

static int to_wide(const char *source, wchar_t **result) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        source, -1, NULL, 0);
    *result = NULL;
    if (count <= 0) return 0;
    *result = calloc((size_t)count, sizeof(**result));
    if (*result == NULL || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            source, -1, *result, count) != count) {
        free(*result);
        *result = NULL;
        return 0;
    }
    return count;
}

static int check_child(unsigned index) {
    wchar_t path[32768];
    wchar_t *command;
    wchar_t *quoted_wide = NULL;
    STARTUPINFOW start = {0};
    PROCESS_INFORMATION process = {0};
    DWORD size, code = 1;
    size_t length, capacity;
    int wide_count;
    int prefix;
    str quoted = xrtShellwordsQuoteWindows(words[index]);
    if (quoted == NULL) return 0;
    size = GetModuleFileNameW(NULL, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    if (size == 0 || size >= sizeof(path) / sizeof(path[0]) - 1) {
        xrtFree(quoted);
        return 0;
    }
    /* Win32 命令行是 UTF-16；验证转换后由宽字符 CRT 读回的真实 argv。 */
    wide_count = to_wide(quoted, &quoted_wide);
    if (wide_count == 0) { xrtFree(quoted); return 0; }
    length = (size_t)wide_count - 1u;
    capacity = (size_t)size + length + 64u;
    command = calloc(capacity, sizeof(*command));
    if (command == NULL) { free(quoted_wide); xrtFree(quoted); return 0; }
    prefix = swprintf(command, capacity, L"\"%ls\" --child %u ", path, index);
    if (prefix < 0 || (size_t)prefix + length >= capacity) goto done;
    wmemcpy(command + prefix, quoted_wide, length);
    start.cb = sizeof(start);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
            NULL, NULL, &start, &process)) goto done;
    if (WaitForSingleObject(process.hProcess, 10000u) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1u);
        WaitForSingleObject(process.hProcess, INFINITE);
    }
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
done:
    free(command);
    free(quoted_wide);
    xrtFree(quoted);
    return code == 0;
}

int wmain(int argc, wchar_t **argv) {
    unsigned i;
    if (argc > 1 && wcscmp(argv[1], L"--child") == 0) {
        wchar_t *end;
        wchar_t *expected;
        unsigned long index;
        int equal;
        if (argc != 4) return 1;
        index = wcstoul(argv[2], &end, 10);
        if (*end != 0 || index >= sizeof(words) / sizeof(words[0]) ||
            !to_wide(words[index], &expected)) return 1;
        equal = wcscmp(argv[3], expected) == 0;
        free(expected);
        return equal ? 0 : 1;
    }
    for (i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (!check_child(i)) {
            fprintf(stderr, "Windows argv round trip failed at case %u\n", i);
            return 1;
        }
    }
    return 0;
}
