/* Real embedded Win32 SDK consumer. No fallback flag, runtime provider or
 * replacement Unicode implementation is permitted in this test. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef WC_ERR_INVALID_CHARS
#error WC_ERR_INVALID_CHARS is missing from the embedded Win32 SDK
#elif WC_ERR_INVALID_CHARS != 0x00000080
#error WC_ERR_INVALID_CHARS does not match the Windows SDK value
#endif

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

long long xl6StdlibWinNlsProbe(void)
{
    /* Taking the address requires the real SDK declaration; an implicit C
     * function declaration cannot accidentally satisfy this ABI check. */
    int (WINAPI *ordinal)(LPCWCH, int, LPCWCH, int, WINBOOL) = CompareStringOrdinal;
    const WCHAR text[] = {'A', 0, 0x4f60, 0xd83d, 0xde42};
    const WCHAR upper[] = {'A', 0, 'B'}, lower[] = {'a', 0, 'b'}, next[] = {'A', 0, 'C'};
    const unsigned char expected[] = {'A', 0, 0xe4, 0xbd, 0xa0,
        0xf0, 0x9f, 0x99, 0x82};
    const WCHAR invalid[][2] = {{0xd800, 'A'}, {0xdc00, 'A'}, {'A', 0xd800}};
    unsigned char output[sizeof expected + 2];
    CHECK(ordinal(upper, 3, lower, 3, TRUE) == CSTR_EQUAL);
    CHECK(ordinal(upper, 3, lower, 3, FALSE) == CSTR_LESS_THAN);
    CHECK(ordinal(upper, 3, next, 3, FALSE) == CSTR_LESS_THAN);
    CHECK(ordinal(upper, 1, next, 1, FALSE) == CSTR_EQUAL);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, 5,
        NULL, 0, NULL, NULL) == (int)sizeof expected);
    memset(output, 0xcc, sizeof output);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, 5,
        (char*)output + 1, sizeof expected, NULL, NULL) == (int)sizeof expected);
    CHECK(!memcmp(output + 1, expected, sizeof expected));
    CHECK(output[0] == 0xcc && output[sizeof output - 1] == 0xcc);
    SetLastError(0);
    CHECK(!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, 5,
        (char*)output + 1, sizeof expected - 1, NULL, NULL));
    CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    CHECK(output[0] == 0xcc && output[sizeof output - 1] == 0xcc);
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        SetLastError(0);
        CHECK(!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, invalid[i],
            2, (char*)output, sizeof output, NULL, NULL));
        CHECK(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    }
    /* A byte/UTF-16 extent must not silently truncate a surrogate pair. */
    SetLastError(0);
    CHECK(!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text + 3, 1,
        (char*)output, sizeof output, NULL, NULL));
    CHECK(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text + 3, 2,
        (char*)output, sizeof output, NULL, NULL) == 4);
    CHECK(!memcmp(output, expected + 5, 4));
    /* Control: omitting strict validation really does replace a lone high
     * surrogate. Keep this distinct from the checked rejection above. */
    CHECK(WideCharToMultiByte(CP_UTF8, 0, invalid[0], 1,
        (char*)output, sizeof output, NULL, NULL) == 3);
    CHECK(output[0] == 0xef && output[1] == 0xbf && output[2] == 0xbd);
    return 0;
}

int main(void)
{
    long long result = xl6StdlibWinNlsProbe();
    if (result) { fprintf(stderr, "Windows NLS SDK failure at %lld\n", result); return 1; }
    puts("Windows NLS SDK: PASS (NUL, UTF-8, surrogate pairs, strict rejection)");
    return 0;
}
